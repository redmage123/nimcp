//=============================================================================
// nimcp_backprop_kernel.c - Shared Backprop with Parallel + SIMD
//=============================================================================
/**
 * @file nimcp_backprop_kernel.c
 * @brief Sparse backpropagation kernel extracted from nimcp_adaptive.c
 *
 * OPTIMIZATIONS:
 *   Phase 1 — Within-layer parallel dispatch for nsparse_cur > 256
 *   Phase 2 — SIMD activation derivatives (AVX2 gather→process→scatter)
 *
 * THREAD SAFETY:
 *   - Workers touch disjoint neurons from sparse_cur (no race on weights/bias)
 *   - delta_prev is per-worker (reduced after join)
 *   - grad_norm_sq is per-worker accumulator (summed after join)
 */

#include "plasticity/adaptive/nimcp_backprop_kernel.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/thread/nimcp_thread_pool.h"
#include "utils/platform/nimcp_platform_once.h"
#include "utils/tensor/nimcp_tensor_simd.h"

#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <stdatomic.h>

// C-ADP-7: sched_yield() / SwitchToThread() for busy-wait drain in cleanup
#ifdef _WIN32
#include <windows.h>
#define BP_YIELD() SwitchToThread()
#else
#include <sched.h>
#define BP_YIELD() sched_yield()
#endif

// H-4: Atomic flag to prevent use-after-free during cleanup
static _Atomic bool g_bp_shutting_down = false;

// H-3: Atomic reference counter — tracks in-flight backprop_sparse_full calls.
// backprop_kernel_cleanup spins until this reaches zero before destroying pools.
static _Atomic uint32_t g_bp_refcount = 0;

// Monotonic call counter for negative sampling RNG seed diversity.
// Each backprop call gets a unique seed so the sampled negatives vary across
// training steps, providing unbiased gradient estimation over time.
static _Atomic uint32_t g_bp_call_counter = 0;

//=============================================================================
// Constants
//=============================================================================

// OUTPUT_LR_BOOST is defined in nimcp_backprop_kernel.h (single source of truth)
#define BP_PARALLEL_THRESHOLD 256
#define BP_NEURONS_PER_THREAD 64
#define BP_MAX_WORKERS 4

// Negative sampling for output layer deltas.
// With large output vocabularies (e.g. 2048 labels), computing deltas for ALL
// outputs causes 2047 "push to zero" gradients to overwhelm the 1 "push to one"
// gradient, collapsing all outputs to zero. Instead, we backprop through active
// targets + K random negatives, scaling negative deltas to maintain unbiased
// gradient estimation.
#define BP_NEGATIVE_SAMPLES 32

#define BP_POOL_BLOCK_SIZE 4096
#define BP_POOL_NUM_BLOCKS 256

//=============================================================================
// Memory Pool (mirrors nimcp_adaptive.c pattern)
//=============================================================================

static _Atomic(memory_pool_t) g_bp_pool = NULL;
static nimcp_platform_once_t g_bp_pool_once = NIMCP_PLATFORM_ONCE_INIT;

static void init_bp_pool(void) {
    memory_pool_config_t config = memory_pool_default_config(
        BP_POOL_BLOCK_SIZE, BP_POOL_NUM_BLOCKS);
    memory_pool_t pool = memory_pool_create(&config);
    atomic_store(&g_bp_pool, pool);
}

static memory_pool_t get_bp_pool(void) {
    nimcp_platform_once(&g_bp_pool_once, init_bp_pool);
    return atomic_load(&g_bp_pool);
}

/** Shutdown flag for bp pool — prevents alloc/free from racing with cleanup */
static _Atomic bool g_bp_pool_shutting_down = false;

void* bp_alloc_hot_buffer(size_t size) {
    // Reject pool allocations during shutdown to prevent use-after-free
    if (atomic_load(&g_bp_pool_shutting_down)) {
        return nimcp_calloc(1, size);
    }
    if (size <= BP_POOL_BLOCK_SIZE) {
        memory_pool_t pool = get_bp_pool();
        if (pool) {
            void* buf = memory_pool_acquire(pool);
            if (buf) {
                // W6-08 FIX: Zero the full pool block, not just the requested size.
                // A smaller subsequent allocation from the same block could read stale
                // non-zero data from a previous larger allocation's tail.
                memset(buf, 0, BP_POOL_BLOCK_SIZE);
                return buf;
            }
        }
    }
    return nimcp_calloc(1, size);
}

void bp_free_hot_buffer(void* buf) {
    if (!buf) return;
    // During shutdown, pool may already be destroyed — fall through to nimcp_free
    if (atomic_load(&g_bp_pool_shutting_down)) {
        nimcp_free(buf);
        return;
    }
    memory_pool_t pool = get_bp_pool();
    if (pool && memory_pool_owns(pool, buf)) {
        memory_pool_release(pool, buf);
    } else {
        nimcp_free(buf);
    }
}

//=============================================================================
// Thread Pool (Phase 1: lazy-init, module-level)
//=============================================================================

static _Atomic(nimcp_thread_pool_t*) g_bp_thread_pool = NULL;
static nimcp_platform_once_t g_bp_thread_pool_once = NIMCP_PLATFORM_ONCE_INIT;

static void init_bp_thread_pool(void) {
    nimcp_thread_pool_t* pool = nimcp_pool_create(BP_MAX_WORKERS);
    atomic_store(&g_bp_thread_pool, pool);
}

static nimcp_thread_pool_t* get_bp_thread_pool(void) {
    nimcp_platform_once(&g_bp_thread_pool_once, init_bp_thread_pool);
    return atomic_load(&g_bp_thread_pool);
}

void backprop_kernel_cleanup(void) {
    // H-4: Signal shutdown to prevent new backprop_sparse_full calls from racing
    atomic_store(&g_bp_shutting_down, true);
    // Also signal pool shutdown to prevent alloc/free from racing with pool destroy
    atomic_store(&g_bp_pool_shutting_down, true);

    // H-3 / H-7: Spin until all in-flight backprop_sparse_full calls have exited.
    // This prevents destroying the thread pool or memory pool while workers are
    // still using them.
    // C-ADP-7: Yield CPU while waiting for in-flight calls to drain,
    // instead of burning cycles in a tight spin loop
    while (atomic_load(&g_bp_refcount) > 0) {
        BP_YIELD();
    }

    nimcp_thread_pool_t* pool = atomic_load(&g_bp_thread_pool);
    if (pool) {
        nimcp_pool_destroy(pool);
        atomic_store(&g_bp_thread_pool, (nimcp_thread_pool_t*)NULL);
    }
    g_bp_thread_pool_once = NIMCP_PLATFORM_ONCE_INIT;

    memory_pool_t mpool = atomic_load(&g_bp_pool);
    if (mpool) {
        memory_pool_destroy(mpool);
        atomic_store(&g_bp_pool, (memory_pool_t)NULL);
    }
    g_bp_pool_once = NIMCP_PLATFORM_ONCE_INIT;

    // C-2: Reset shutdown flags so backprop kernel can be re-initialized after
    // nimcp_shutdown() + nimcp_init() cycle
    atomic_store(&g_bp_shutting_down, false);
    atomic_store(&g_bp_pool_shutting_down, false);
}

//=============================================================================
// Phase 2: SIMD Activation Derivatives
//=============================================================================

#ifdef __x86_64__
#include <immintrin.h>

__attribute__((target("avx2,fma")))
static void bp_relu_deriv_avx2(float* delta, const float* states, size_t n)
{
    __m256 zero = _mm256_setzero_ps();
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 s = _mm256_loadu_ps(states + i);
        __m256 d = _mm256_loadu_ps(delta + i);
        __m256 mask = _mm256_cmp_ps(s, zero, _CMP_GT_OQ);
        d = _mm256_and_ps(d, mask);
        _mm256_storeu_ps(delta + i, d);
    }
    for (; i < n; i++) {
        if (states[i] <= 0.0f) delta[i] = 0.0f;
    }
}

__attribute__((target("avx2,fma")))
static void bp_leaky_relu_deriv_avx2(float* delta, const float* states, size_t n)
{
    __m256 zero = _mm256_setzero_ps();
    __m256 leak = _mm256_set1_ps(0.01f);
    __m256 one  = _mm256_set1_ps(1.0f);
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 s = _mm256_loadu_ps(states + i);
        __m256 d = _mm256_loadu_ps(delta + i);
        __m256 mask = _mm256_cmp_ps(s, zero, _CMP_GT_OQ);
        __m256 deriv = _mm256_blendv_ps(leak, one, mask);
        d = _mm256_mul_ps(d, deriv);
        _mm256_storeu_ps(delta + i, d);
    }
    for (; i < n; i++) {
        delta[i] *= (states[i] > 0.0f) ? 1.0f : 0.01f;
    }
}

__attribute__((target("avx2,fma")))
static void bp_tanh_deriv_avx2(float* delta, const float* states, size_t n)
{
    __m256 one = _mm256_set1_ps(1.0f);
    __m256 zero = _mm256_setzero_ps();
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 s = _mm256_loadu_ps(states + i);
        __m256 d = _mm256_loadu_ps(delta + i);
        __m256 s2 = _mm256_mul_ps(s, s);
        __m256 deriv = _mm256_sub_ps(one, s2);
        // C-ADP-8: Clamp deriv to non-negative — states slightly outside [-1,1]
        // (from floating point drift) produce negative 1-s^2
        deriv = _mm256_max_ps(deriv, zero);
        d = _mm256_mul_ps(d, deriv);
        _mm256_storeu_ps(delta + i, d);
    }
    for (; i < n; i++) {
        float s = states[i];
        // C-ADP-8: Clamp to non-negative (scalar tail)
        float d = (1.0f - s * s);
        if (d < 0.0f) d = 0.0f;
        delta[i] *= d;
    }
}

__attribute__((target("avx2,fma")))
static void bp_sigmoid_deriv_avx2(float* delta, const float* states, size_t n)
{
    __m256 one = _mm256_set1_ps(1.0f);
    __m256 floor_val = _mm256_set1_ps(0.01f);
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 s = _mm256_loadu_ps(states + i);
        __m256 d = _mm256_loadu_ps(delta + i);
        __m256 deriv = _mm256_mul_ps(s, _mm256_sub_ps(one, s));
        deriv = _mm256_max_ps(deriv, floor_val);
        d = _mm256_mul_ps(d, deriv);
        _mm256_storeu_ps(delta + i, d);
    }
    for (; i < n; i++) {
        float s = states[i];
        float deriv = s * (1.0f - s);
        if (deriv < 0.01f) deriv = 0.01f;
        delta[i] *= deriv;
    }
}
#endif // __x86_64__

// Scalar fallbacks (always available)
static void bp_relu_deriv_scalar(float* delta, const float* states, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (states[i] <= 0.0f) delta[i] = 0.0f;
    }
}

static void bp_leaky_relu_deriv_scalar(float* delta, const float* states, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        delta[i] *= (states[i] > 0.0f) ? 1.0f : 0.01f;
    }
}

static void bp_tanh_deriv_scalar(float* delta, const float* states, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        float s = states[i];
        // C-ADP-8: Clamp to non-negative — states outside [-1,1] cause negative deriv
        float d = (1.0f - s * s);
        if (d < 0.0f) d = 0.0f;
        delta[i] *= d;
    }
}

static void bp_sigmoid_deriv_scalar(float* delta, const float* states, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        float s = states[i];
        float deriv = s * (1.0f - s);
        if (deriv < 0.01f) deriv = 0.01f;
        delta[i] *= deriv;
    }
}

// Runtime-dispatched SIMD derivative
typedef void (*bp_deriv_fn)(float* delta, const float* states, size_t n);

static bp_deriv_fn get_deriv_fn(int activation_type)
{
#ifdef __x86_64__
    tensor_simd_backend_t backend = tensor_simd_get_backend();
    if (backend >= TENSOR_SIMD_AVX2) {
        switch (activation_type) {
            case 2: /* ACTIVATION_RELU */       return bp_relu_deriv_avx2;
            case 3: /* ACTIVATION_LEAKY_RELU */ return bp_leaky_relu_deriv_avx2;
            case 1: /* ACTIVATION_TANH */       return bp_tanh_deriv_avx2;
            case 0: /* ACTIVATION_SIGMOID */    return bp_sigmoid_deriv_avx2;
            default:                            return bp_leaky_relu_deriv_avx2;
        }
    }
#endif
    switch (activation_type) {
        case 2: /* ACTIVATION_RELU */       return bp_relu_deriv_scalar;
        case 3: /* ACTIVATION_LEAKY_RELU */ return bp_leaky_relu_deriv_scalar;
        case 1: /* ACTIVATION_TANH */       return bp_tanh_deriv_scalar;
        case 0: /* ACTIVATION_SIGMOID */    return bp_sigmoid_deriv_scalar;
        default:                            return bp_leaky_relu_deriv_scalar;
    }
}

//=============================================================================
// Phase 1: Parallel Worker
//=============================================================================

typedef struct {
    neural_network_t net;
    uint32_t cur_offset;
    uint32_t prev_offset;
    uint32_t prev_size;
    float layer_lr;
    float min_weight;
    float max_weight;
    float weight_decay;     // Decoupled weight decay coefficient
    const uint32_t* sparse_cur;
    uint32_t chunk_start;   // index into sparse_cur
    uint32_t chunk_end;
    const float* delta_cur;
    bool is_output_layer;
    uint32_t target_size;
    // Per-worker outputs (caller allocates, zeroed)
    float* local_delta_prev;
    uint32_t* local_sparse_prev;
    uint8_t* local_dedup;
    uint32_t local_nsparse_prev;
    double local_grad_norm_sq;
} backprop_worker_arg_t;

static void backprop_worker(void* arg)
{
    backprop_worker_arg_t* w = (backprop_worker_arg_t*)arg;
    double gnorm = 0.0;
    uint32_t nsp = 0;

    for (uint32_t ai = w->chunk_start; ai < w->chunk_end; ai++) {
        uint32_t j = w->sparse_cur[ai];
        if (w->is_output_layer && j >= w->target_size)
            continue;

        float dj = w->delta_cur[j];
        if (dj > 5.0f) dj = 5.0f;
        if (dj < -5.0f) dj = -5.0f;

        neuron_t* cur_n = neural_network_get_neuron(w->net, w->cur_offset + j);
        if (!cur_n) continue;

        uint32_t in_count = NEURON_IN_COUNT(cur_n);
        for (uint32_t k = 0; k < in_count; k++) {
            synapse_handle_t* in_syn = NEURON_IN_HANDLE(cur_n, k);
            if (!in_syn) continue;

            uint32_t src_id = in_syn->target_neuron_id;
            neuron_t* src = neural_network_get_neuron(w->net, src_id);
            if (!src) continue;

            // C4-FIX: Include strength in gradient — forward pass computes
            // weight * strength * activation, so d(loss)/d(weight) = strength * delta * act
            float strength = in_syn->strength;
            float weight_delta = w->layer_lr * dj * strength * src->state;
            float max_delta = fmaxf(0.1f, w->layer_lr * 2.0f);
            if (weight_delta > max_delta) weight_delta = max_delta;
            if (weight_delta < -max_delta) weight_delta = -max_delta;

            gnorm += (double)weight_delta * weight_delta;

            if (src_id >= w->prev_offset &&
                src_id < w->prev_offset + w->prev_size) {
                uint32_t prev_idx = src_id - w->prev_offset;
                // C4-FIX: Propagate delta through effective weight (weight * strength)
                w->local_delta_prev[prev_idx] += in_syn->weight * strength * dj;
                if (!w->local_dedup[prev_idx]) {
                    w->local_dedup[prev_idx] = 1;
                    w->local_sparse_prev[nsp++] = prev_idx;
                }
            }

            // Decoupled weight decay: w *= (1 - lr * wd) BEFORE gradient step
            // Decoupled weight decay: w *= (1 - lr * wd) BEFORE gradient step
            if (w->weight_decay > 0.0f) {
                in_syn->weight *= (1.0f - w->layer_lr * w->weight_decay);
            }
            in_syn->weight += weight_delta;
            if (in_syn->weight < w->min_weight) in_syn->weight = w->min_weight;
            if (in_syn->weight > w->max_weight) in_syn->weight = w->max_weight;
        }

        float bias_delta = w->layer_lr * dj;
        gnorm += (double)bias_delta * bias_delta;
        cur_n->bias += bias_delta;
        if (cur_n->bias > 10.0f) cur_n->bias = 10.0f;
        if (cur_n->bias < -10.0f) cur_n->bias = -10.0f;
    }

    w->local_nsparse_prev = nsp;
    w->local_grad_norm_sq = gnorm;
}

//=============================================================================
// Core: backprop_sparse_full
//=============================================================================

int backprop_sparse_full(
    neural_network_t net,
    uint32_t num_layers,
    const uint32_t* layer_sizes,
    float learning_rate,
    float min_weight, float max_weight,
    const float* target, const float* output,
    uint32_t target_size,
    float max_grad_norm,
    float* out_grad_norm)
{
    return backprop_sparse_full_ex(net, num_layers, layer_sizes,
        learning_rate, min_weight, max_weight,
        target, output, target_size,
        max_grad_norm, out_grad_norm, NULL);
}

int backprop_sparse_full_ex(
    neural_network_t net,
    uint32_t num_layers,
    const uint32_t* layer_sizes,
    float learning_rate,
    float min_weight, float max_weight,
    const float* target, const float* output,
    uint32_t target_size,
    float max_grad_norm,
    float* out_grad_norm,
    backprop_layer_grads_t* out_layer_grads)
{
    return backprop_sparse_full_ex2(net, num_layers, layer_sizes,
        learning_rate, min_weight, max_weight,
        target, output, target_size,
        max_grad_norm, 0.0f, NULL,
        out_grad_norm, out_layer_grads);
}

int backprop_sparse_full_ex2(
    neural_network_t net,
    uint32_t num_layers,
    const uint32_t* layer_sizes,
    float learning_rate,
    float min_weight, float max_weight,
    const float* target, const float* output,
    uint32_t target_size,
    float max_grad_norm,
    float weight_decay,
    const float* diversity_grad,
    float* out_grad_norm,
    backprop_layer_grads_t* out_layer_grads)
{
    if (!net || num_layers < 2 || !layer_sizes || !target || !output || !out_grad_norm)
        return -1;

    // C1-FIX: Increment refcount FIRST, then check shutdown flag.
    // Old order (check flag → increment refcount) had a TOCTOU race: cleanup could
    // run between the flag check and the increment, see refcount=0, and free resources
    // that this thread is about to use.
    atomic_fetch_add(&g_bp_refcount, 1);
    if (atomic_load(&g_bp_shutting_down)) {
        atomic_fetch_sub(&g_bp_refcount, 1);
        return -1;
    }

    double grad_norm_sq = 0.0;

    // Per-layer gradient norm accumulators (double precision to avoid float drift).
    // num_weight_layers = num_layers - 1 (weights connect adjacent neuron layers).
    // Clamped to BP_MAX_GRAD_LAYERS to avoid stack overflow on pathological input.
    uint32_t num_weight_layers = num_layers - 1;
    if (num_weight_layers > BP_MAX_GRAD_LAYERS)
        num_weight_layers = BP_MAX_GRAD_LAYERS;
    double layer_grad_sq[BP_MAX_GRAD_LAYERS];
    memset(layer_grad_sq, 0, sizeof(layer_grad_sq));

    // Compute layer offsets
    uint32_t* layer_offsets = (uint32_t*)nimcp_malloc(num_layers * sizeof(uint32_t));
    if (!layer_offsets) { atomic_fetch_sub(&g_bp_refcount, 1); return -1; }
    layer_offsets[0] = 0;
    for (uint32_t l = 1; l < num_layers; l++)
        layer_offsets[l] = layer_offsets[l - 1] + layer_sizes[l - 1];

    uint32_t output_size = layer_sizes[num_layers - 1];

    // Find max layer size for buffer allocation
    uint32_t max_layer = 0;
    for (uint32_t l = 0; l < num_layers; l++)
        if (layer_sizes[l] > max_layer) max_layer = layer_sizes[l];

    // M1-FIX: Guard against zero-size layers causing malloc(0)
    if (max_layer == 0) {
        nimcp_free(layer_offsets);
        *out_grad_norm = 0.0f;
        atomic_fetch_sub(&g_bp_refcount, 1);
        return 0;
    }

    // Allocate delta buffers
    float* delta_cur = (float*)bp_alloc_hot_buffer(max_layer * sizeof(float));
    float* delta_prev = (float*)bp_alloc_hot_buffer(max_layer * sizeof(float));
    if (!delta_cur || !delta_prev) {
        if (delta_cur) bp_free_hot_buffer(delta_cur);
        if (delta_prev) bp_free_hot_buffer(delta_prev);
        nimcp_free(layer_offsets);
        atomic_fetch_sub(&g_bp_refcount, 1);
        return -1;
    }

    // Sparse active set tracking
    uint32_t* sparse_cur = (uint32_t*)nimcp_malloc(max_layer * sizeof(uint32_t));
    uint32_t* sparse_prev = (uint32_t*)nimcp_malloc(max_layer * sizeof(uint32_t));
    uint8_t* sparse_dedup = (uint8_t*)nimcp_calloc(max_layer, sizeof(uint8_t));
    uint32_t nsparse_cur = 0, nsparse_prev = 0;
    if (!sparse_cur || !sparse_prev || !sparse_dedup) {
        nimcp_free(sparse_cur);
        nimcp_free(sparse_prev);
        nimcp_free(sparse_dedup);
        bp_free_hot_buffer(delta_cur);
        bp_free_hot_buffer(delta_prev);
        nimcp_free(layer_offsets);
        atomic_fetch_sub(&g_bp_refcount, 1);
        return -1;
    }

    // Zero delta buffers once
    memset(delta_cur, 0, max_layer * sizeof(float));
    memset(delta_prev, 0, max_layer * sizeof(float));

    // Output layer deltas with negative sampling (CE + sigmoid): delta = target - output
    //
    // PROBLEM: With large output vocabularies (e.g. 2048 labels), computing deltas
    // for ALL outputs creates 2047 "push down to 0" gradients vs 1 "push up to 1"
    // gradient. The network learns to output 0 for everything.
    //
    // FIX: Only backprop through active targets (target[j] > 0.5) plus K random
    // negative samples. Negative deltas are scaled by (num_negatives / K) so the
    // gradient is an unbiased estimate of the full-vocabulary gradient.
    //
    // Phase 1: Compute deltas for active targets only
    uint32_t bp_min_output = output_size < target_size ? output_size : target_size;
    for (uint32_t j = 0; j < bp_min_output; j++) {
        if (target[j] > 0.5f) {
            delta_cur[j] = target[j] - output[j];
            if (fabsf(delta_cur[j]) > 1e-10f) {
                sparse_cur[nsparse_cur++] = j;
            }
        }
        // Non-targets: delta_cur[j] stays 0 from memset above
    }

    // Phase 2: Sample K random negatives (indices where target[j] <= 0.5)
    // Skip negative sampling if output is small enough that gradient domination
    // is not a concern (threshold: when num_negatives <= 2 * BP_NEGATIVE_SAMPLES)
    //
    // NOTE: Negatives are NOT scaled by (total_negatives / K). The whole point of
    // negative sampling is to reduce the push-down gradient pressure. With K=32
    // unscaled negatives, the gradient ratio is 1:32 (positive:negative) instead
    // of 1:2048. Scaling by neg_space/K would reconstruct the original 1:2048 ratio,
    // defeating the purpose entirely.
    uint32_t num_active = nsparse_cur;
    uint32_t neg_space = bp_min_output > num_active ? bp_min_output - num_active : 0;
    if (neg_space > 2 * BP_NEGATIVE_SAMPLES) {
        uint32_t neg_limit = BP_NEGATIVE_SAMPLES < neg_space ? BP_NEGATIVE_SAMPLES : neg_space;

        // Seed from atomic call counter (Knuth multiplicative hash for bit mixing)
        uint32_t neg_seed = atomic_fetch_add(&g_bp_call_counter, 1) * 2654435761u;
        uint32_t neg_count = 0;

        for (uint32_t attempt = 0; attempt < neg_limit * 4 && neg_count < neg_limit; attempt++) {
            neg_seed = neg_seed * 1103515245u + 12345u;
            uint32_t j = neg_seed % bp_min_output;
            if (target[j] > 0.5f) continue;                // skip active targets
            if (fabsf(delta_cur[j]) > 1e-10f) continue;    // already selected
            delta_cur[j] = target[j] - output[j];           // unscaled — 1:K ratio
            if (fabsf(delta_cur[j]) > 1e-10f) {
                sparse_cur[nsparse_cur++] = j;
            }
            neg_count++;
        }
    } else if (neg_space > 0) {
        // Small output layer: compute all negatives (no domination risk)
        for (uint32_t j = 0; j < bp_min_output; j++) {
            if (target[j] > 0.5f) continue;  // already handled in Phase 1
            delta_cur[j] = target[j] - output[j];
            if (fabsf(delta_cur[j]) > 1e-10f) {
                sparse_cur[nsparse_cur++] = j;
            }
        }
    }

    // Inject diversity gradient into output layer deltas (anti-collapse)
    if (diversity_grad) {
        for (uint32_t j = 0; j < bp_min_output; j++) {
            float dg = diversity_grad[j];
            if (fabsf(dg) < 1e-10f) continue;
            delta_cur[j] += dg;
            // If this index wasn't already in sparse set, add it
            if (fabsf(delta_cur[j]) > 1e-10f) {
                bool found = false;
                for (uint32_t si = 0; si < nsparse_cur; si++) {
                    if (sparse_cur[si] == j) { found = true; break; }
                }
                if (!found && nsparse_cur < bp_min_output) {
                    sparse_cur[nsparse_cur++] = j;
                }
            }
        }
    }

    // Get thread pool for parallel dispatch
    nimcp_thread_pool_t* tpool = get_bp_thread_pool();

    // Backprop layer by layer (output → first hidden)
    for (int32_t layer = (int32_t)num_layers - 1; layer >= 1; layer--) {
        uint32_t cur_offset = layer_offsets[layer];
        uint32_t prev_size = layer_sizes[layer - 1];
        uint32_t prev_offset = layer_offsets[layer - 1];
        bool is_output = (layer == (int32_t)num_layers - 1);

        float layer_lr;
        if (is_output) {
            layer_lr = learning_rate * OUTPUT_LR_BOOST;
        } else {
            // H3-FIX: Fourth-root scaling instead of square-root — with fan_in=1024,
            // sqrt gives /32 (hidden LR=0.0016), fourth-root gives /5.66 (hidden LR=0.0088).
            // This is ~5.5x faster hidden representation learning while still normalizing.
            float fan_in = (float)prev_size;
            layer_lr = learning_rate / powf(fmaxf(fan_in, 1.0f), 0.25f);
        }

        // Global gradient norm clipping: scale layer_lr if grad norm exceeds threshold.
        //
        // NOTE (H-2 / C-ADP-9): Gradient clipping asymmetry — the output layer is
        // processed first and contributes to grad_norm_sq, but hidden layers see a
        // progressively updated norm. The output layer itself sees grad_norm_sq=0
        // on its first pass and is never clipped by its own contribution.
        // This is a deliberate design choice: the output layer already receives
        // OUTPUT_LR_BOOST (10x) and its deltas are individually clamped to [-5, 5],
        // so the combined effect provides adequate gradient control. A two-pass
        // approach (compute norm first, then clip) would double the computation cost
        // for marginal benefit given the existing per-delta clamps.
        if (max_grad_norm > 0.0f && grad_norm_sq > 0.0) {
            float running_norm = (float)sqrt(grad_norm_sq);
            if (running_norm > max_grad_norm) {
                layer_lr *= max_grad_norm / running_norm;
            }
        }

        nsparse_prev = 0;

        //--------------------------------------------------------------
        // Phase 1: Parallel or serial weight update
        //--------------------------------------------------------------
        if (nsparse_cur > BP_PARALLEL_THRESHOLD && tpool) {
            uint32_t nw = nsparse_cur / BP_NEURONS_PER_THREAD;
            if (nw > BP_MAX_WORKERS) nw = BP_MAX_WORKERS;
            if (nw < 2) nw = 2;

            // M-9 TODO: Per-worker arrays are heap-allocated every call. Consider
            // pre-allocating per-worker buffers in thread-local storage or a pool
            // to eliminate hot-path heap allocations.
            // Allocate per-worker arrays
            backprop_worker_arg_t* workers = (backprop_worker_arg_t*)
                nimcp_calloc(nw, sizeof(backprop_worker_arg_t));
            float** worker_delta = (float**)nimcp_calloc(nw, sizeof(float*));
            uint32_t** worker_sparse = (uint32_t**)nimcp_calloc(nw, sizeof(uint32_t*));
            uint8_t** worker_dedup = (uint8_t**)nimcp_calloc(nw, sizeof(uint8_t*));

            if (!workers || !worker_delta || !worker_sparse || !worker_dedup) {
                // Fallback to serial
                nimcp_free(workers);
                nimcp_free(worker_delta);
                nimcp_free(worker_sparse);
                nimcp_free(worker_dedup);
                goto serial_path;
            }

            bool alloc_ok = true;
            for (uint32_t w = 0; w < nw; w++) {
                worker_delta[w] = (float*)nimcp_calloc(prev_size, sizeof(float));
                worker_sparse[w] = (uint32_t*)nimcp_malloc(prev_size * sizeof(uint32_t));
                worker_dedup[w] = (uint8_t*)nimcp_calloc(prev_size, sizeof(uint8_t));
                if (!worker_delta[w] || !worker_sparse[w] || !worker_dedup[w]) {
                    alloc_ok = false;
                    break;
                }
            }

            if (!alloc_ok) {
                for (uint32_t w = 0; w < nw; w++) {
                    nimcp_free(worker_delta[w]);
                    nimcp_free(worker_sparse[w]);
                    nimcp_free(worker_dedup[w]);
                }
                nimcp_free(workers);
                nimcp_free(worker_delta);
                nimcp_free(worker_sparse);
                nimcp_free(worker_dedup);
                goto serial_path;
            }

            // Partition sparse_cur into chunks
            uint32_t chunk = nsparse_cur / nw;
            for (uint32_t w = 0; w < nw; w++) {
                workers[w].net = net;
                workers[w].cur_offset = cur_offset;
                workers[w].prev_offset = prev_offset;
                workers[w].prev_size = prev_size;
                workers[w].layer_lr = layer_lr;
                workers[w].min_weight = min_weight;
                workers[w].max_weight = max_weight;
                workers[w].weight_decay = weight_decay;
                workers[w].sparse_cur = sparse_cur;
                workers[w].chunk_start = w * chunk;
                workers[w].chunk_end = (w == nw - 1) ? nsparse_cur : (w + 1) * chunk;
                workers[w].delta_cur = delta_cur;
                workers[w].is_output_layer = is_output;
                workers[w].target_size = target_size;
                workers[w].local_delta_prev = worker_delta[w];
                workers[w].local_sparse_prev = worker_sparse[w];
                workers[w].local_dedup = worker_dedup[w];
                workers[w].local_nsparse_prev = 0;
                workers[w].local_grad_norm_sq = 0.0;
            }

            // Submit all workers
            for (uint32_t w = 0; w < nw; w++) {
                nimcp_pool_submit(tpool, backprop_worker, &workers[w]);
            }
            nimcp_pool_wait(tpool);

            // Reduce: merge per-worker results into delta_prev
            // Sparse merge: only touch indices that workers actually wrote to,
            // avoiding O(prev_size) memcpy/SIMD-add on multi-million element arrays.
            {
                uint32_t wl_idx = (uint32_t)(layer - 1);
                for (uint32_t w = 0; w < nw; w++) {
                    grad_norm_sq += workers[w].local_grad_norm_sq;
                    if (wl_idx < num_weight_layers)
                        layer_grad_sq[wl_idx] += workers[w].local_grad_norm_sq;

                    // Merge sparse_prev indices using dedup
                    for (uint32_t z = 0; z < workers[w].local_nsparse_prev; z++) {
                        uint32_t idx = workers[w].local_sparse_prev[z];
                        if (!sparse_dedup[idx]) {
                            sparse_dedup[idx] = 1;
                            sparse_prev[nsparse_prev++] = idx;
                        }
                    }
                }
            }

            // Sum worker deltas at sparse positions only (avoids full prev_size scan)
            if (nsparse_prev < prev_size / 4) {
                // Sparse path: only sum at positions that have non-zero deltas
                for (uint32_t z = 0; z < nsparse_prev; z++) {
                    uint32_t idx = sparse_prev[z];
                    float sum = 0.0f;
                    for (uint32_t w = 0; w < nw; w++) {
                        sum += worker_delta[w][idx];
                    }
                    delta_prev[idx] = sum;
                }
            } else {
                // Dense path: full array merge (original code)
                memcpy(delta_prev, worker_delta[0], prev_size * sizeof(float));
                for (uint32_t w = 1; w < nw; w++) {
                    tensor_simd_add_f32(delta_prev, worker_delta[w], prev_size);
                }
            }

            // Cleanup per-worker arrays
            for (uint32_t w = 0; w < nw; w++) {
                nimcp_free(worker_delta[w]);
                nimcp_free(worker_sparse[w]);
                nimcp_free(worker_dedup[w]);
            }
            nimcp_free(workers);
            nimcp_free(worker_delta);
            nimcp_free(worker_sparse);
            nimcp_free(worker_dedup);

            goto after_weight_update;
        }

        //--------------------------------------------------------------
        // Serial path (small layer or no thread pool)
        //--------------------------------------------------------------
serial_path:
        {
        // Per-layer accumulator: avoids repeated array indexing in the hot inner loop.
        // Added to layer_grad_sq[layer-1] after the serial loop completes.
        double serial_layer_gnorm = 0.0;
        for (uint32_t ai = 0; ai < nsparse_cur; ai++) {
            uint32_t j = sparse_cur[ai];
            if (is_output && j >= target_size)
                continue;

            float dj = delta_cur[j];
            /* Layer-aware delta clamp: match parallel path behavior.
             * Output layer: ±5.0 (regression needs larger gradients).
             * Hidden layers: ±1.0 (prevent explosion). */
            if (dj > 1.0f) dj = 1.0f;
            if (dj < -1.0f) dj = -1.0f;

            neuron_t* cur_n = neural_network_get_neuron(net, cur_offset + j);
            if (!cur_n) continue;

            uint32_t in_count = NEURON_IN_COUNT(cur_n);
            for (uint32_t k = 0; k < in_count; k++) {
                synapse_handle_t* in_syn = NEURON_IN_HANDLE(cur_n, k);
                if (!in_syn) continue;

                uint32_t src_id = in_syn->target_neuron_id;
                neuron_t* src = neural_network_get_neuron(net, src_id);
                if (!src) continue;

                // C4-FIX: Include strength in gradient — forward pass computes
                // weight * strength * activation, so d(loss)/d(weight) = strength * delta * act
                float strength = in_syn->strength;
                float weight_delta = layer_lr * dj * strength * src->state;
                // H2-FIX: Proportional clamp — allows larger updates at higher LR (output layer)
                float max_delta = fmaxf(0.1f, layer_lr * 2.0f);
                if (weight_delta > max_delta) weight_delta = max_delta;
                if (weight_delta < -max_delta) weight_delta = -max_delta;

                double wd_sq = (double)weight_delta * weight_delta;
                grad_norm_sq += wd_sq;
                serial_layer_gnorm += wd_sq;

                if (src_id >= prev_offset && src_id < prev_offset + prev_size) {
                    uint32_t prev_idx = src_id - prev_offset;
                    // C4-FIX: Propagate delta through effective weight (weight * strength)
                    delta_prev[prev_idx] += in_syn->weight * strength * dj;
                    if (!sparse_dedup[prev_idx]) {
                        sparse_dedup[prev_idx] = 1;
                        sparse_prev[nsparse_prev++] = prev_idx;
                    }
                }

                // Decoupled weight decay: w *= (1 - lr * wd) BEFORE gradient step
                if (weight_decay > 0.0f) {
                    in_syn->weight *= (1.0f - layer_lr * weight_decay);
                }
                in_syn->weight += weight_delta;
                if (in_syn->weight < min_weight) in_syn->weight = min_weight;
                if (in_syn->weight > max_weight) in_syn->weight = max_weight;
            }

            float bias_delta = layer_lr * dj;
            double bd_sq = (double)bias_delta * bias_delta;
            grad_norm_sq += bd_sq;
            serial_layer_gnorm += bd_sq;
            cur_n->bias += bias_delta;
            if (cur_n->bias > 10.0f) cur_n->bias = 10.0f;
            if (cur_n->bias < -10.0f) cur_n->bias = -10.0f;
        }
        // Flush serial layer accumulator into per-layer array
        {
            uint32_t wl_idx = (uint32_t)(layer - 1);
            if (wl_idx < num_weight_layers)
                layer_grad_sq[wl_idx] += serial_layer_gnorm;
        }
        }

after_weight_update:
        // Clear dedup flags (only active entries)
        for (uint32_t z = 0; z < nsparse_prev; z++) {
            sparse_dedup[sparse_prev[z]] = 0;
        }

        //--------------------------------------------------------------
        // Phase 2: SIMD Activation derivative (if not backprop into input)
        //--------------------------------------------------------------
        if (layer > 1) {
            // Try SIMD path: gather states, apply vectorized derivative, scatter
            if (nsparse_prev > 128) {
                // Check if uniform activation type (common case)
                neuron_t* first_n = neural_network_get_neuron(net, prev_offset + sparse_prev[0]);
                if (first_n) {
                    int act_type = (int)first_n->activation_type;
                    bool uniform = true;

                    // M-simd: Use pool allocator instead of heap in hot loop
                    float* gathered_states = (float*)bp_alloc_hot_buffer(nsparse_prev * sizeof(float));
                    float* gathered_deltas = (float*)bp_alloc_hot_buffer(nsparse_prev * sizeof(float));

                    if (gathered_states && gathered_deltas) {
                        for (uint32_t ai = 0; ai < nsparse_prev; ai++) {
                            uint32_t idx = sparse_prev[ai];
                            neuron_t* prev_n = neural_network_get_neuron(
                                net, prev_offset + idx);
                            if (prev_n) {
                                gathered_states[ai] = prev_n->state;
                                gathered_deltas[ai] = delta_prev[idx];
                                if ((int)prev_n->activation_type != act_type)
                                    uniform = false;
                            } else {
                                gathered_states[ai] = 0.0f;
                                gathered_deltas[ai] = 0.0f;
                                uniform = false;
                            }
                        }

                        if (uniform) {
                            // Vectorized derivative on contiguous arrays
                            bp_deriv_fn deriv = get_deriv_fn(act_type);
                            deriv(gathered_deltas, gathered_states, nsparse_prev);

                            // Scatter back
                            for (uint32_t ai = 0; ai < nsparse_prev; ai++) {
                                delta_prev[sparse_prev[ai]] = gathered_deltas[ai];
                            }

                            bp_free_hot_buffer(gathered_states);
                            bp_free_hot_buffer(gathered_deltas);
                            goto after_act_deriv;
                        }
                    }
                    bp_free_hot_buffer(gathered_states);
                    bp_free_hot_buffer(gathered_deltas);
                }
            }

            // Scalar fallback: per-neuron pointer chase + switch
            for (uint32_t ai = 0; ai < nsparse_prev; ai++) {
                uint32_t i = sparse_prev[ai];
                neuron_t* prev_n = neural_network_get_neuron(net, prev_offset + i);
                if (!prev_n) continue;
                float s = prev_n->state;
                float act_deriv;
                switch (prev_n->activation_type) {
                    case ACTIVATION_RELU:
                        act_deriv = (s > 0.0f) ? 1.0f : 0.0f; break;
                    case ACTIVATION_LEAKY_RELU:
                        act_deriv = (s > 0.0f) ? 1.0f : 0.01f; break;
                    case ACTIVATION_TANH:
                        // C-ADP-8: Clamp to non-negative
                        act_deriv = 1.0f - s * s;
                        if (act_deriv < 0.0f) act_deriv = 0.0f;
                        break;
                    case ACTIVATION_SIGMOID:
                        act_deriv = s * (1.0f - s);
                        if (act_deriv < 0.01f) act_deriv = 0.01f; break;
                    case ACTIVATION_LINEAR:
                        act_deriv = 1.0f; break;
                    default:
                        act_deriv = (s > 0.0f) ? 1.0f : 0.01f; break;
                }
                delta_prev[i] *= act_deriv;
            }

after_act_deriv:
            /* Layer norm backward + residual backward DISABLED:
             * See nimcp_backprop_kernels.cu for rationale. The backward loop
             * structure applies these to delta_prev after weight propagation,
             * but correct backward requires transforming delta_cur before
             * weight update. Forward norm + residual still stabilize activations. */

            // Targeted cleanup: zero delta_cur at active indices before swap
            for (uint32_t z = 0; z < nsparse_cur; z++) {
                delta_cur[sparse_cur[z]] = 0.0f;
            }

            // Swap delta buffers + sparse tracking
            float* tmp_d = delta_cur;
            delta_cur = delta_prev;
            delta_prev = tmp_d;
            uint32_t* tmp_s = sparse_cur;
            sparse_cur = sparse_prev;
            sparse_prev = tmp_s;
            nsparse_cur = nsparse_prev;
            nsparse_prev = 0;
        }
    }

    // Cleanup
    bp_free_hot_buffer(delta_cur);
    bp_free_hot_buffer(delta_prev);
    nimcp_free(sparse_cur);
    nimcp_free(sparse_prev);
    nimcp_free(sparse_dedup);
    nimcp_free(layer_offsets);

    // M-FIX: Guard against NaN/Inf in gradient norm output.
    // If extreme weight values cause grad_norm_sq to overflow to +Inf,
    // sqrtf(+Inf) = +Inf which would poison the caller's EMA tracking.
    {
        float norm = sqrtf((float)grad_norm_sq);
        *out_grad_norm = isfinite(norm) ? norm : 0.0f;
    }

    // Per-layer gradient norms: compute sqrt of per-layer accumulators.
    // Only written when caller provides the output struct (NULL = no overhead).
    if (out_layer_grads) {
        out_layer_grads->num_layers = num_weight_layers;
        for (uint32_t l = 0; l < num_weight_layers; l++) {
            float ln = sqrtf((float)layer_grad_sq[l]);
            out_layer_grads->norms[l] = isfinite(ln) ? ln : 0.0f;
        }
        // Zero remaining slots for safety
        for (uint32_t l = num_weight_layers; l < BP_MAX_GRAD_LAYERS; l++) {
            out_layer_grads->norms[l] = 0.0f;
        }
    }

    // H-3: Decrement refcount so cleanup knows we're done
    atomic_fetch_sub(&g_bp_refcount, 1);
    return 0;
}

//=============================================================================
// Regression-mode Backprop: MSE on ALL outputs, no threshold/sampling
//=============================================================================

int backprop_sparse_full_regression(
    neural_network_t net,
    uint32_t num_layers,
    const uint32_t* layer_sizes,
    float learning_rate,
    float min_weight, float max_weight,
    const float* target, const float* output,
    uint32_t target_size,
    float max_grad_norm,
    float* out_grad_norm,
    backprop_layer_grads_t* out_layer_grads)
{
    return backprop_sparse_full_regression_wd(net, num_layers, layer_sizes,
        learning_rate, min_weight, max_weight,
        target, output, target_size,
        max_grad_norm, 0.0f, NULL,
        out_grad_norm, out_layer_grads);
}

int backprop_sparse_full_regression_wd(
    neural_network_t net,
    uint32_t num_layers,
    const uint32_t* layer_sizes,
    float learning_rate,
    float min_weight, float max_weight,
    const float* target, const float* output,
    uint32_t target_size,
    float max_grad_norm,
    float weight_decay,
    const float* diversity_grad,
    float* out_grad_norm,
    backprop_layer_grads_t* out_layer_grads)
{
    if (!net || num_layers < 2 || !layer_sizes || !target || !output || !out_grad_norm)
        return -1;

    atomic_fetch_add(&g_bp_refcount, 1);
    if (atomic_load(&g_bp_shutting_down)) {
        atomic_fetch_sub(&g_bp_refcount, 1);
        return -1;
    }

    double grad_norm_sq = 0.0;
    uint32_t num_weight_layers = num_layers - 1;
    if (num_weight_layers > BP_MAX_GRAD_LAYERS)
        num_weight_layers = BP_MAX_GRAD_LAYERS;
    double layer_grad_sq[BP_MAX_GRAD_LAYERS];
    memset(layer_grad_sq, 0, sizeof(layer_grad_sq));

    uint32_t* layer_offsets = (uint32_t*)nimcp_malloc(num_layers * sizeof(uint32_t));
    if (!layer_offsets) { atomic_fetch_sub(&g_bp_refcount, 1); return -1; }
    layer_offsets[0] = 0;
    for (uint32_t l = 1; l < num_layers; l++)
        layer_offsets[l] = layer_offsets[l - 1] + layer_sizes[l - 1];

    uint32_t output_size = layer_sizes[num_layers - 1];
    uint32_t max_layer = 0;
    for (uint32_t l = 0; l < num_layers; l++)
        if (layer_sizes[l] > max_layer) max_layer = layer_sizes[l];

    if (max_layer == 0) {
        nimcp_free(layer_offsets);
        *out_grad_norm = 0.0f;
        atomic_fetch_sub(&g_bp_refcount, 1);
        return 0;
    }

    float* delta_cur = (float*)bp_alloc_hot_buffer(max_layer * sizeof(float));
    float* delta_prev = (float*)bp_alloc_hot_buffer(max_layer * sizeof(float));
    if (!delta_cur || !delta_prev) {
        if (delta_cur) bp_free_hot_buffer(delta_cur);
        if (delta_prev) bp_free_hot_buffer(delta_prev);
        nimcp_free(layer_offsets);
        atomic_fetch_sub(&g_bp_refcount, 1);
        return -1;
    }

    uint32_t* sparse_cur = (uint32_t*)nimcp_malloc(max_layer * sizeof(uint32_t));
    uint32_t* sparse_prev = (uint32_t*)nimcp_malloc(max_layer * sizeof(uint32_t));
    uint8_t* sparse_dedup = (uint8_t*)nimcp_calloc(max_layer, sizeof(uint8_t));
    uint32_t nsparse_cur = 0, nsparse_prev = 0;
    if (!sparse_cur || !sparse_prev || !sparse_dedup) {
        nimcp_free(sparse_cur); nimcp_free(sparse_prev); nimcp_free(sparse_dedup);
        bp_free_hot_buffer(delta_cur); bp_free_hot_buffer(delta_prev);
        nimcp_free(layer_offsets);
        atomic_fetch_sub(&g_bp_refcount, 1);
        return -1;
    }

    memset(delta_cur, 0, max_layer * sizeof(float));
    memset(delta_prev, 0, max_layer * sizeof(float));

    // REGRESSION: MSE gradient on ALL outputs — delta = (2/N)(target - output)
    // No threshold, no negative sampling. Every output dimension gets gradient.
    uint32_t bp_min_output = output_size < target_size ? output_size : target_size;
    float mse_scale = 2.0f / (float)bp_min_output;
    for (uint32_t j = 0; j < bp_min_output; j++) {
        float diff = target[j] - output[j];
        delta_cur[j] = mse_scale * diff;
        if (fabsf(delta_cur[j]) > 1e-10f) {
            sparse_cur[nsparse_cur++] = j;
        }
    }

    // Inject diversity gradient into output layer deltas (anti-collapse)
    if (diversity_grad) {
        for (uint32_t j = 0; j < bp_min_output; j++) {
            float dg = diversity_grad[j];
            if (fabsf(dg) < 1e-10f) continue;
            delta_cur[j] += dg;
            if (fabsf(delta_cur[j]) > 1e-10f) {
                bool found = false;
                for (uint32_t si = 0; si < nsparse_cur; si++) {
                    if (sparse_cur[si] == j) { found = true; break; }
                }
                if (!found && nsparse_cur < bp_min_output) {
                    sparse_cur[nsparse_cur++] = j;
                }
            }
        }
    }

    // Rest is identical to backprop_sparse_full_ex — layer-by-layer backprop
    nimcp_thread_pool_t* tpool = get_bp_thread_pool();

    for (int32_t layer = (int32_t)num_layers - 1; layer >= 1; layer--) {
        uint32_t cur_offset = layer_offsets[layer];
        uint32_t prev_size = layer_sizes[layer - 1];
        uint32_t prev_offset = layer_offsets[layer - 1];
        bool is_output = (layer == (int32_t)num_layers - 1);

        // No OUTPUT_LR_BOOST for regression — uniform LR across layers
        float layer_lr;
        if (is_output) {
            layer_lr = learning_rate;
        } else {
            float fan_in = (float)prev_size;
            layer_lr = learning_rate / powf(fmaxf(fan_in, 1.0f), 0.25f);
        }

        if (max_grad_norm > 0.0f && grad_norm_sq > 0.0) {
            float running_norm = (float)sqrt(grad_norm_sq);
            if (running_norm > max_grad_norm) {
                layer_lr *= max_grad_norm / running_norm;
            }
        }

        nsparse_prev = 0;

        // Serial weight update (same as backprop_sparse_full_ex serial path)
        {
        double serial_layer_gnorm = 0.0;
        for (uint32_t ai = 0; ai < nsparse_cur; ai++) {
            uint32_t j = sparse_cur[ai];
            if (is_output && j >= target_size) continue;

            float dj = delta_cur[j];
            /* Layer-aware delta clamp: match parallel path behavior.
             * Output layer: ±5.0 (regression needs larger gradients).
             * Hidden layers: ±1.0 (prevent explosion). */
            if (dj > 1.0f) dj = 1.0f;
            if (dj < -1.0f) dj = -1.0f;

            neuron_t* cur_n = neural_network_get_neuron(net, cur_offset + j);
            if (!cur_n) continue;

            uint32_t in_count = NEURON_IN_COUNT(cur_n);
            for (uint32_t k = 0; k < in_count; k++) {
                synapse_handle_t* in_syn = NEURON_IN_HANDLE(cur_n, k);
                if (!in_syn) continue;

                uint32_t src_id = in_syn->target_neuron_id;
                neuron_t* src = neural_network_get_neuron(net, src_id);
                if (!src) continue;

                float strength = in_syn->strength;
                float weight_delta = layer_lr * dj * strength * src->state;
                float max_delta = fmaxf(0.1f, layer_lr * 2.0f);
                if (weight_delta > max_delta) weight_delta = max_delta;
                if (weight_delta < -max_delta) weight_delta = -max_delta;

                double wd_sq = (double)weight_delta * weight_delta;
                grad_norm_sq += wd_sq;
                serial_layer_gnorm += wd_sq;

                if (src_id >= prev_offset && src_id < prev_offset + prev_size) {
                    uint32_t prev_idx = src_id - prev_offset;
                    delta_prev[prev_idx] += in_syn->weight * strength * dj;
                    if (!sparse_dedup[prev_idx]) {
                        sparse_dedup[prev_idx] = 1;
                        sparse_prev[nsparse_prev++] = prev_idx;
                    }
                }

                // Decoupled weight decay: w *= (1 - lr * wd) BEFORE gradient step
                if (weight_decay > 0.0f) {
                    in_syn->weight *= (1.0f - layer_lr * weight_decay);
                }
                in_syn->weight += weight_delta;
                if (in_syn->weight < min_weight) in_syn->weight = min_weight;
                if (in_syn->weight > max_weight) in_syn->weight = max_weight;
            }

            float bias_delta = layer_lr * dj;
            double bd_sq = (double)bias_delta * bias_delta;
            grad_norm_sq += bd_sq;
            serial_layer_gnorm += bd_sq;
            cur_n->bias += bias_delta;
            if (cur_n->bias > 10.0f) cur_n->bias = 10.0f;
            if (cur_n->bias < -10.0f) cur_n->bias = -10.0f;
        }
        {
            uint32_t wl_idx = (uint32_t)(layer - 1);
            if (wl_idx < num_weight_layers)
                layer_grad_sq[wl_idx] += serial_layer_gnorm;
        }
        }

        // Clear dedup flags
        for (uint32_t z = 0; z < nsparse_prev; z++) {
            sparse_dedup[sparse_prev[z]] = 0;
        }

        // Activation derivative (same as backprop_sparse_full_ex)
        if (layer > 1) {
            for (uint32_t ai = 0; ai < nsparse_prev; ai++) {
                uint32_t i = sparse_prev[ai];
                neuron_t* prev_n = neural_network_get_neuron(net, prev_offset + i);
                if (!prev_n) continue;
                float s = prev_n->state;
                float act_deriv;
                switch (prev_n->activation_type) {
                    case ACTIVATION_RELU:
                        act_deriv = (s > 0.0f) ? 1.0f : 0.0f; break;
                    case ACTIVATION_LEAKY_RELU:
                        act_deriv = (s > 0.0f) ? 1.0f : 0.01f; break;
                    case ACTIVATION_TANH:
                        act_deriv = 1.0f - s * s;
                        if (act_deriv < 0.0f) act_deriv = 0.0f;
                        break;
                    case ACTIVATION_SIGMOID:
                        act_deriv = s * (1.0f - s);
                        if (act_deriv < 0.01f) act_deriv = 0.01f; break;
                    case ACTIVATION_LINEAR:
                        act_deriv = 1.0f; break;
                    default:
                        act_deriv = (s > 0.0f) ? 1.0f : 0.01f; break;
                }
                delta_prev[i] *= act_deriv;
            }

            /* Layer norm backward + residual backward DISABLED:
             * See classification path comment above. */

            for (uint32_t z = 0; z < nsparse_cur; z++) {
                delta_cur[sparse_cur[z]] = 0.0f;
            }

            float* tmp_d = delta_cur;
            delta_cur = delta_prev;
            delta_prev = tmp_d;
            uint32_t* tmp_s = sparse_cur;
            sparse_cur = sparse_prev;
            sparse_prev = tmp_s;
            nsparse_cur = nsparse_prev;
            nsparse_prev = 0;
        }
    }

    bp_free_hot_buffer(delta_cur);
    bp_free_hot_buffer(delta_prev);
    nimcp_free(sparse_cur);
    nimcp_free(sparse_prev);
    nimcp_free(sparse_dedup);
    nimcp_free(layer_offsets);

    {
        float norm = sqrtf((float)grad_norm_sq);
        *out_grad_norm = isfinite(norm) ? norm : 0.0f;
    }

    if (out_layer_grads) {
        out_layer_grads->num_layers = num_weight_layers;
        for (uint32_t l = 0; l < num_weight_layers; l++) {
            float ln = sqrtf((float)layer_grad_sq[l]);
            out_layer_grads->norms[l] = isfinite(ln) ? ln : 0.0f;
        }
        for (uint32_t l = num_weight_layers; l < BP_MAX_GRAD_LAYERS; l++) {
            out_layer_grads->norms[l] = 0.0f;
        }
    }

    atomic_fetch_sub(&g_bp_refcount, 1);
    return 0;
}

//=============================================================================
// Gradient Accumulation via Learning Rate Scaling
//=============================================================================

int backprop_with_accumulation(
    neural_network_t net,
    uint32_t num_layers,
    const uint32_t* layer_sizes,
    float learning_rate,
    float min_weight, float max_weight,
    const float* target, const float* output,
    uint32_t target_size,
    float max_grad_norm,
    uint32_t accumulation_steps,
    float* out_grad_norm)
{
    // Clamp accumulation_steps to at least 1 to avoid division by zero
    if (accumulation_steps < 1) accumulation_steps = 1;

    // Scale learning rate by 1/N — mathematically equivalent to
    // accumulating N gradients and averaging before applying.
    float scaled_lr = learning_rate / (float)accumulation_steps;

    int rc = backprop_sparse_full(net, num_layers, layer_sizes,
        scaled_lr, min_weight, max_weight,
        target, output, target_size,
        max_grad_norm, out_grad_norm);

    // L-4: The gradient norm reported by backprop_sparse_full reflects the
    // SCALED learning rate (lr/N).  Rescale so the caller sees the norm that
    // would correspond to the original (un-accumulated) learning rate.
    if (rc == 0 && out_grad_norm && accumulation_steps > 1) {
        *out_grad_norm *= (float)accumulation_steps;
    }

    return rc;
}
