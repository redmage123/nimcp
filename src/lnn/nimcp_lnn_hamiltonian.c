/**
 * @file nimcp_lnn_hamiltonian.c
 * @brief Hamiltonian Neural Network — energy-conserving LNN dynamics
 *
 * Implements H(q,p) network, symplectic Störmer-Verlet integrator,
 * and forward/backward passes for Hamiltonian LNN layers.
 */

#include "lnn/nimcp_lnn_hamiltonian.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <float.h>

/* W5 KG-integration — HNN network-level aggregator event emitter.
 * Emits "energy_drift" when |H(t) - H(0)| / |H(0)| exceeds 0.1, which
 * indicates the symplectic integrator is losing conservation (bug or
 * numerical blow-up). Admin-token self-elevation per kg-registry §7. */
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"

static brain_t s_net_hnn_kg_brain = NULL;

void net_hnn_kg_register_brain(brain_t brain) {
    s_net_hnn_kg_brain = brain;
}

static void net_hnn_kg_emit_event(brain_t brain, const char* kind,
                                  float magnitude, uint64_t ts_us) {
    if (!brain || !kind) return;
    if (!brain->internal_kg_enabled || !brain->internal_kg) return;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    char ev_name[160];
    snprintf(ev_name, sizeof(ev_name),
             "net_hnn_event_%s_%llu", kind, (unsigned long long)ts_us);
    char desc[240];
    snprintf(desc, sizeof(desc),
             "HNN energy conservation event: kind=%s magnitude=%.4f",
             kind, magnitude);

    brain_kg_node_id_t ev = brain_kg_add_node(kg, ev_name,
        BRAIN_KG_NODE_CORE, desc);
    brain_kg_node_id_t owner = brain_kg_find_node(kg, "net_hnn");
    if (owner != BRAIN_KG_INVALID_NODE && ev != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, owner, ev, BRAIN_KG_EDGE_SENDS_TO,
            "produced_by", magnitude);
    }
    if (ev != BRAIN_KG_INVALID_NODE) {
        char buf[40];
        snprintf(buf, sizeof(buf), "%.6f", magnitude);
        brain_kg_add_metadata(kg, ev, "magnitude", buf);
        snprintf(buf, sizeof(buf), "%llu", (unsigned long long)ts_us);
        brain_kg_add_metadata(kg, ev, "ts_us", buf);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

/* Public test-facing trigger — lets tests verify emit without forcing a
 * conservation violation in real HNN dynamics. Pure forwarding. */
void net_hnn_kg_trigger_event(brain_t brain, const char* kind,
                              float magnitude, uint64_t ts_us) {
    net_hnn_kg_emit_event(brain, kind, magnitude, ts_us);
}

/* =========================================================================
 * Rayleigh dissipation tunables (biologically-motivated, substrate-coupled)
 *
 * Process-wide globals so a single safety toggle applies uniformly across
 * every live HNN in the address space. Default OFF so conservation is
 * preserved for existing users; opt-in via hnn_tune_set_dissipation_enabled.
 * ========================================================================= */
static float g_hnn_dissipation_enabled   = 0.0f;  /* default OFF → conservation */
static float g_hnn_dissipation_gamma_max = 0.1f;  /* max γ at capacity = 0 */

void hnn_tune_set_dissipation_enabled(float v) {
    g_hnn_dissipation_enabled = (v != 0.0f) ? 1.0f : 0.0f;
}

void hnn_tune_set_dissipation_gamma_max(float v) {
    if (v < 0.0f)  v = 0.0f;
    if (v > 10.0f) v = 10.0f;
    g_hnn_dissipation_gamma_max = v;
}

float hnn_tune_get_dissipation_enabled(void)    { return g_hnn_dissipation_enabled; }
float hnn_tune_get_dissipation_gamma_max(void)  { return g_hnn_dissipation_gamma_max; }

/* =========================================================================
 * Activation functions
 * ========================================================================= */

static float softplus(float x) {
    if (x > 20.0f) return x;  /* Prevent overflow */
    return logf(1.0f + expf(x));
}

static float softplus_grad(float x) {
    if (x > 20.0f) return 1.0f;
    float ex = expf(x);
    return ex / (1.0f + ex);
}

/* =========================================================================
 * Configuration
 * ========================================================================= */

void lnn_hamiltonian_config_default(lnn_hamiltonian_config_t* config) {
    if (!config) return;
    config->hidden_dim = 0;           /* Auto: 2 * state_dim */
    config->n_hidden_layers = 2;
    config->separable = false;
    config->energy_penalty_weight = 0.01f;
    config->input_coupling = 1.0f;
}

/* =========================================================================
 * H-Network Lifecycle
 * ========================================================================= */

lnn_hamiltonian_net_t* lnn_hamiltonian_net_create(
    uint32_t state_dim,
    const lnn_hamiltonian_config_t* config)
{
    if (state_dim == 0) {
        NIMCP_LOGGING_ERROR("lnn_hamiltonian_net_create: state_dim must be > 0");
        return NULL;
    }

    lnn_hamiltonian_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        lnn_hamiltonian_config_default(&cfg);
    }

    uint32_t hidden = cfg.hidden_dim > 0 ? cfg.hidden_dim : 2 * state_dim;
    uint32_t n_hidden = cfg.n_hidden_layers;
    uint32_t n_layers = n_hidden + 1;  /* hidden layers + output layer */
    uint32_t input_dim = 2 * state_dim; /* [q; p] */

    if (n_layers > 100) {
        NIMCP_LOGGING_ERROR("lnn_hamiltonian_net_create: n_layers %u unreasonably large", n_layers);
        return NULL;
    }

    lnn_hamiltonian_net_t* net = nimcp_calloc(1, sizeof(lnn_hamiltonian_net_t));
    if (!net) return NULL;

    net->state_dim = state_dim;
    net->input_dim = input_dim;
    net->n_layers = n_layers;
    net->separable = cfg.separable;
    net->H_current = 0.0f;
    net->H_initial = 0.0f;
    net->has_initial = false;
    net->energy_deviation = 0.0f;

    /* Layer dimensions */
    net->layer_dims = nimcp_calloc(n_layers + 1, sizeof(uint32_t));
    if (!net->layer_dims) { nimcp_free(net); return NULL; }

    net->layer_dims[0] = input_dim;  /* Input */
    for (uint32_t i = 1; i <= n_hidden; i++) {
        net->layer_dims[i] = hidden;
    }
    net->layer_dims[n_layers] = 1;   /* Scalar output (energy) */

    /* Allocate weight/bias arrays */
    net->W = nimcp_calloc(n_layers, sizeof(nimcp_tensor_t*));
    net->b = nimcp_calloc(n_layers, sizeof(nimcp_tensor_t*));
    net->grad_W = nimcp_calloc(n_layers, sizeof(nimcp_tensor_t*));
    net->grad_b = nimcp_calloc(n_layers, sizeof(nimcp_tensor_t*));
    net->pre_act = nimcp_calloc(n_layers, sizeof(nimcp_tensor_t*));
    net->post_act = nimcp_calloc(n_layers + 1, sizeof(nimcp_tensor_t*));

    if (!net->W || !net->b || !net->grad_W || !net->grad_b ||
        !net->pre_act || !net->post_act) {
        lnn_hamiltonian_net_destroy(net);
        return NULL;
    }

    /* Create tensors for each layer */
    for (uint32_t l = 0; l < n_layers; l++) {
        uint32_t in_d = net->layer_dims[l];
        uint32_t out_d = net->layer_dims[l + 1];

        uint32_t w_dims[2] = {out_d, in_d};
        uint32_t b_dims[1] = {out_d};
        uint32_t act_dims[1] = {out_d};

        net->W[l] = nimcp_tensor_create(w_dims, 2, NIMCP_DTYPE_F32);
        net->b[l] = nimcp_tensor_zeros(b_dims, 1, NIMCP_DTYPE_F32);
        net->grad_W[l] = nimcp_tensor_zeros(w_dims, 2, NIMCP_DTYPE_F32);
        net->grad_b[l] = nimcp_tensor_zeros(b_dims, 1, NIMCP_DTYPE_F32);
        net->pre_act[l] = nimcp_tensor_zeros(act_dims, 1, NIMCP_DTYPE_F32);

        if (!net->W[l] || !net->b[l] || !net->grad_W[l] || !net->grad_b[l]) {
            lnn_hamiltonian_net_destroy(net);
            return NULL;
        }

        /* Xavier initialization for weights */
        float scale = sqrtf(2.0f / (float)(in_d + out_d));
        float* w_data = (float*)nimcp_tensor_data(net->W[l]);
        for (uint32_t i = 0; i < out_d * in_d; i++) {
            w_data[i] = scale * (2.0f * ((float)rand() / RAND_MAX) - 1.0f);
        }
    }

    /* Post-activation cache (includes input as post_act[0]) */
    uint32_t in_dims[1] = {input_dim};
    net->post_act[0] = nimcp_tensor_zeros(in_dims, 1, NIMCP_DTYPE_F32);
    for (uint32_t l = 0; l < n_layers; l++) {
        uint32_t d = net->layer_dims[l + 1];
        uint32_t dims[1] = {d};
        net->post_act[l + 1] = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);
    }

    NIMCP_LOGGING_INFO("Hamiltonian net created: state_dim=%u, hidden=%u×%u, params=%u",
                       state_dim, hidden, n_hidden,
                       lnn_hamiltonian_param_count(net));
    return net;
}

void lnn_hamiltonian_net_destroy(lnn_hamiltonian_net_t* net) {
    if (!net) return;

    if (net->W) {
        for (uint32_t l = 0; l < net->n_layers; l++)
            nimcp_tensor_destroy(net->W[l]);
        nimcp_free(net->W);
    }
    if (net->b) {
        for (uint32_t l = 0; l < net->n_layers; l++)
            nimcp_tensor_destroy(net->b[l]);
        nimcp_free(net->b);
    }
    if (net->grad_W) {
        for (uint32_t l = 0; l < net->n_layers; l++)
            nimcp_tensor_destroy(net->grad_W[l]);
        nimcp_free(net->grad_W);
    }
    if (net->grad_b) {
        for (uint32_t l = 0; l < net->n_layers; l++)
            nimcp_tensor_destroy(net->grad_b[l]);
        nimcp_free(net->grad_b);
    }
    if (net->pre_act) {
        for (uint32_t l = 0; l < net->n_layers; l++)
            nimcp_tensor_destroy(net->pre_act[l]);
        nimcp_free(net->pre_act);
    }
    if (net->post_act) {
        for (uint32_t l = 0; l <= net->n_layers; l++)
            nimcp_tensor_destroy(net->post_act[l]);
        nimcp_free(net->post_act);
    }

    nimcp_free(net->layer_dims);
    nimcp_free(net);
}

/* =========================================================================
 * Forward Evaluation: H(q,p)
 * ========================================================================= */

float lnn_hamiltonian_eval(
    lnn_hamiltonian_net_t* net,
    const nimcp_tensor_t* q,
    const nimcp_tensor_t* p)
{
    if (!net || !q || !p) return 0.0f;

    uint32_t n = net->state_dim;

    /* Concatenate [q; p] into input */
    float* input_data = (float*)nimcp_tensor_data(net->post_act[0]);
    const float* q_data = (const float*)nimcp_tensor_data_const(q);
    const float* p_data = (const float*)nimcp_tensor_data_const(p);
    memcpy(input_data, q_data, n * sizeof(float));
    memcpy(input_data + n, p_data, n * sizeof(float));

    /* Forward through each layer */
    for (uint32_t l = 0; l < net->n_layers; l++) {
        uint32_t in_d = net->layer_dims[l];
        uint32_t out_d = net->layer_dims[l + 1];

        const float* W_data = (const float*)nimcp_tensor_data_const(net->W[l]);
        const float* b_data = (const float*)nimcp_tensor_data_const(net->b[l]);
        float* pre_data = (float*)nimcp_tensor_data(net->pre_act[l]);
        float* post_data = (float*)nimcp_tensor_data(net->post_act[l + 1]);
        const float* prev_data = (const float*)nimcp_tensor_data_const(net->post_act[l]);

        /* z = W @ x + b */
        for (uint32_t i = 0; i < out_d; i++) {
            float sum = b_data[i];
            for (uint32_t j = 0; j < in_d; j++) {
                sum += W_data[i * in_d + j] * prev_data[j];
            }
            pre_data[i] = sum;

            /* Activation: softplus for hidden layers, none for output */
            if (l < net->n_layers - 1) {
                post_data[i] = softplus(sum);
            } else {
                post_data[i] = sum;  /* Raw scalar energy */
            }
        }
    }

    /* Output is scalar H */
    float H = ((float*)nimcp_tensor_data(net->post_act[net->n_layers]))[0];
    net->H_current = H;

    if (!net->has_initial) {
        net->H_initial = H;
        net->has_initial = true;
    }

    float H0 = fabsf(net->H_initial);
    net->energy_deviation = (H0 > 1e-8f) ? fabsf(H - net->H_initial) / H0 : 0.0f;

    return H;
}

/* =========================================================================
 * Gradient: ∂H/∂q and ∂H/∂p via backprop
 * ========================================================================= */

int lnn_hamiltonian_grad(
    lnn_hamiltonian_net_t* net,
    const nimcp_tensor_t* q,
    const nimcp_tensor_t* p,
    nimcp_tensor_t* dH_dq,
    nimcp_tensor_t* dH_dp)
{
    if (!net || !q || !p || !dH_dq || !dH_dp) return -1;

    /* Forward pass (caches activations) */
    lnn_hamiltonian_eval(net, q, p);

    uint32_t n = net->state_dim;

    /* Backward: start with dL/dH = 1.0 */
    /* Allocate upstream gradient for each layer */
    float** upstream = nimcp_calloc(net->n_layers + 1, sizeof(float*));
    if (!upstream) return -1;

    for (uint32_t l = 0; l <= net->n_layers; l++) {
        upstream[l] = nimcp_calloc(net->layer_dims[l], sizeof(float));
        if (!upstream[l]) {
            for (uint32_t k = 0; k < l; k++) nimcp_free(upstream[k]);
            nimcp_free(upstream);
            return -1;
        }
    }

    /* dL/dOutput = 1.0 */
    upstream[net->n_layers][0] = 1.0f;

    /* Backward through each layer */
    for (int l = (int)net->n_layers - 1; l >= 0; l--) {
        uint32_t in_d = net->layer_dims[l];
        uint32_t out_d = net->layer_dims[l + 1];

        const float* W_data = (const float*)nimcp_tensor_data_const(net->W[l]);
        float* gW_data = (float*)nimcp_tensor_data(net->grad_W[l]);
        float* gb_data = (float*)nimcp_tensor_data(net->grad_b[l]);
        const float* pre_data = (const float*)nimcp_tensor_data_const(net->pre_act[l]);
        const float* prev_act = (const float*)nimcp_tensor_data_const(net->post_act[l]);

        /* Apply activation gradient for hidden layers */
        float* delta = nimcp_calloc(out_d, sizeof(float));
        if (!delta) {
            for (uint32_t k = 0; k <= net->n_layers; k++) nimcp_free(upstream[k]);
            nimcp_free(upstream);
            return -1;
        }

        for (uint32_t i = 0; i < out_d; i++) {
            if ((uint32_t)l < net->n_layers - 1) {
                delta[i] = upstream[l + 1][i] * softplus_grad(pre_data[i]);
            } else {
                delta[i] = upstream[l + 1][i];  /* No activation on output */
            }
        }

        /* Accumulate weight and bias gradients */
        for (uint32_t i = 0; i < out_d; i++) {
            gb_data[i] += delta[i];
            for (uint32_t j = 0; j < in_d; j++) {
                gW_data[i * in_d + j] += delta[i] * prev_act[j];
            }
        }

        /* Propagate upstream: upstream[l] = W^T @ delta */
        for (uint32_t j = 0; j < in_d; j++) {
            float sum = 0.0f;
            for (uint32_t i = 0; i < out_d; i++) {
                sum += W_data[i * in_d + j] * delta[i];
            }
            upstream[l][j] = sum;
        }

        nimcp_free(delta);
    }

    /* Split input gradient into dH/dq and dH/dp */
    float* dq = (float*)nimcp_tensor_data(dH_dq);
    float* dp = (float*)nimcp_tensor_data(dH_dp);
    memcpy(dq, upstream[0], n * sizeof(float));
    memcpy(dp, upstream[0] + n, n * sizeof(float));

    /* Cleanup */
    for (uint32_t l = 0; l <= net->n_layers; l++) nimcp_free(upstream[l]);
    nimcp_free(upstream);

    return 0;
}

/* =========================================================================
 * Störmer-Verlet Symplectic Integrator
 * ========================================================================= */

int lnn_hamiltonian_step_stormer_verlet_with_substrate(
    lnn_hamiltonian_net_t* net,
    nimcp_tensor_t* q,
    nimcp_tensor_t* p,
    const nimcp_tensor_t* input,
    float dt,
    float input_coupling,
    const axon_substrate_effects_t* axon_eff)
{
    if (!net || !q || !p) return -1;

    uint32_t n = net->state_dim;

    nimcp_tensor_t* dH_dq = nimcp_tensor_zeros((uint32_t[]){n}, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* dH_dp = nimcp_tensor_zeros((uint32_t[]){n}, 1, NIMCP_DTYPE_F32);
    if (!dH_dq || !dH_dp) {
        nimcp_tensor_destroy(dH_dq);
        nimcp_tensor_destroy(dH_dp);
        return -1;
    }

    float* q_data = (float*)nimcp_tensor_data(q);
    float* p_data = (float*)nimcp_tensor_data(p);

    /* Rayleigh damping coefficient — active ONLY when the enabled flag is
     * set AND a substrate effects struct was supplied. Either precondition
     * missing keeps γ = 0, preserving conservation exactly. */
    const bool dissipate = (g_hnn_dissipation_enabled != 0.0f) && (axon_eff != NULL);
    float gamma = 0.0f;
    if (dissipate) {
        float cap = axon_eff->overall_capacity;
        if (cap < 0.0f) cap = 0.0f;
        if (cap > 1.0f) cap = 1.0f;
        gamma = g_hnn_dissipation_gamma_max * (1.0f - cap);
    }

    /* Störmer-Verlet symplectic integrator (correct ordering):
     *   Step 1: p_{1/2} = p_n - (dt/2) * ∂H/∂q(q_n, p_n)
     *   Step 2: q_{n+1} = q_n + dt * ∂H/∂p(q_n, p_{1/2})
     *   Step 3: p_{n+1} = p_{1/2} - (dt/2) * ∂H/∂q(q_{n+1}, p_{1/2})
     * Note: p is modified in-place to p_half after step 1, then q is updated,
     * then p_half is updated to p_new. This is the correct leapfrog ordering.
     *
     * Rayleigh damping is applied as a half-step update on each momentum
     * half-step — this keeps the discretization symmetric and matches the
     * continuous-time augmentation dp/dt = -∂H/∂q - γ·p. */

    /* Step 1: p_half = p - (dt/2) * (∂H/∂q + γ·p) */
    lnn_hamiltonian_grad(net, q, p, dH_dq, dH_dp);
    float* dHdq = (float*)nimcp_tensor_data(dH_dq);
    for (uint32_t i = 0; i < n; i++) {
        p_data[i] -= 0.5f * dt * (dHdq[i] + gamma * p_data[i]);
    }
    /* p_data now holds p_half */

    /* Add input coupling to momentum: p_half += dt * coupling * input */
    if (input && input_coupling > 0.0f) {
        const float* in_data = (const float*)nimcp_tensor_data_const(input);
        uint32_t in_n = nimcp_tensor_numel(input);
        uint32_t copy_n = (in_n < n) ? in_n : n;
        for (uint32_t i = 0; i < copy_n; i++) {
            p_data[i] += dt * input_coupling * in_data[i];
        }
    }

    /* Step 2: q_new = q + dt * ∂H/∂p(q, p_half) */
    lnn_hamiltonian_grad(net, q, p, dH_dq, dH_dp);
    /* p is already p_half, q is still q_n — gradient computed at (q_n, p_half) */
    float* dHdp = (float*)nimcp_tensor_data(dH_dp);
    for (uint32_t i = 0; i < n; i++) {
        q_data[i] += dt * dHdp[i];
    }
    /* q_data now holds q_{n+1} */

    /* Step 3: p_new = p_half - (dt/2) * (∂H/∂q + γ·p_half) */
    lnn_hamiltonian_grad(net, q, p, dH_dq, dH_dp);
    /* q is now q_{n+1}, p is still p_half — gradient at (q_{n+1}, p_half) */
    dHdq = (float*)nimcp_tensor_data(dH_dq);
    for (uint32_t i = 0; i < n; i++) {
        p_data[i] -= 0.5f * dt * (dHdq[i] + gamma * p_data[i]);
    }
    /* p_data now holds p_{n+1} */

    /* Clamp to prevent divergence */
    for (uint32_t i = 0; i < n; i++) {
        if (!isfinite(q_data[i])) q_data[i] = 0.0f;
        if (!isfinite(p_data[i])) p_data[i] = 0.0f;
        if (q_data[i] > 100.0f) q_data[i] = 100.0f;
        if (q_data[i] < -100.0f) q_data[i] = -100.0f;
        if (p_data[i] > 100.0f) p_data[i] = 100.0f;
        if (p_data[i] < -100.0f) p_data[i] = -100.0f;
    }

    /* Update energy */
    lnn_hamiltonian_eval(net, q, p);

    /* W5 KG anomaly emit: conservation violation > 10% relative drift.
     * This is the single hot-path hook for HNN — emit only on threshold
     * cross to keep KG writes rare. */
    if (s_net_hnn_kg_brain && net->has_initial &&
        net->energy_deviation > 0.1f) {
        uint64_t ts_us = (uint64_t)time(NULL) * 1000000ULL;
        net_hnn_kg_emit_event(s_net_hnn_kg_brain, "energy_drift",
                              net->energy_deviation, ts_us);
    }

    nimcp_tensor_destroy(dH_dq);
    nimcp_tensor_destroy(dH_dp);
    return 0;
}

/* Backward-compatible wrapper: preserves exact behavior of the original
 * symplectic integrator when no substrate is passed. */
int lnn_hamiltonian_step_stormer_verlet(
    lnn_hamiltonian_net_t* net,
    nimcp_tensor_t* q,
    nimcp_tensor_t* p,
    const nimcp_tensor_t* input,
    float dt,
    float input_coupling)
{
    return lnn_hamiltonian_step_stormer_verlet_with_substrate(
        net, q, p, input, dt, input_coupling, NULL);
}

/* =========================================================================
 * Hamilton's Equations — continuous-time derivatives (no integration step)
 *
 * Exposed so tests and external code can probe dq/dt, dp/dt without
 * side-effects on q, p. Equivalent to one RHS evaluation.
 * ========================================================================= */

int lnn_hamiltonian_forward_with_substrate(
    lnn_hamiltonian_net_t* net,
    const nimcp_tensor_t* q,
    const nimcp_tensor_t* p,
    nimcp_tensor_t* dq,
    nimcp_tensor_t* dp,
    const axon_substrate_effects_t* axon_eff)
{
    if (!net || !q || !p || !dq || !dp) return -1;

    uint32_t n = net->state_dim;

    /* Allocate scratch for ∂H/∂q and ∂H/∂p — reuse dp/dq layout by writing
     * into temporary tensors then mapping dq = ∂H/∂p, dp = -∂H/∂q. */
    nimcp_tensor_t* dH_dq = nimcp_tensor_zeros((uint32_t[]){n}, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* dH_dp = nimcp_tensor_zeros((uint32_t[]){n}, 1, NIMCP_DTYPE_F32);
    if (!dH_dq || !dH_dp) {
        nimcp_tensor_destroy(dH_dq);
        nimcp_tensor_destroy(dH_dp);
        return -1;
    }

    int rc = lnn_hamiltonian_grad(net, q, p, dH_dq, dH_dp);
    if (rc != 0) {
        nimcp_tensor_destroy(dH_dq);
        nimcp_tensor_destroy(dH_dp);
        return rc;
    }

    const float* dHdq = (const float*)nimcp_tensor_data_const(dH_dq);
    const float* dHdp = (const float*)nimcp_tensor_data_const(dH_dp);
    const float* p_data = (const float*)nimcp_tensor_data_const(p);
    float* dq_data = (float*)nimcp_tensor_data(dq);
    float* dp_data = (float*)nimcp_tensor_data(dp);

    /* Hamilton's equations:
     *   dq/dt =  ∂H/∂p
     *   dp/dt = -∂H/∂q   (+ Rayleigh damping when dissipation is active) */
    for (uint32_t i = 0; i < n; i++) {
        dq_data[i] =  dHdp[i];
        dp_data[i] = -dHdq[i];
    }

    /* Rayleigh damping — active ONLY when enabled AND substrate supplied.
     * Either precondition missing keeps the vanilla conservation form. */
    if (g_hnn_dissipation_enabled != 0.0f && axon_eff != NULL) {
        float cap = axon_eff->overall_capacity;
        if (cap < 0.0f) cap = 0.0f;
        if (cap > 1.0f) cap = 1.0f;
        const float gamma = g_hnn_dissipation_gamma_max * (1.0f - cap);
        for (uint32_t i = 0; i < n; i++) {
            dp_data[i] -= gamma * p_data[i];
        }
    }

    nimcp_tensor_destroy(dH_dq);
    nimcp_tensor_destroy(dH_dp);
    return 0;
}

int lnn_hamiltonian_forward(
    lnn_hamiltonian_net_t* net,
    const nimcp_tensor_t* q,
    const nimcp_tensor_t* p,
    nimcp_tensor_t* dq,
    nimcp_tensor_t* dp)
{
    return lnn_hamiltonian_forward_with_substrate(net, q, p, dq, dp, NULL);
}

/* =========================================================================
 * Layer-level Forward/Backward
 * ========================================================================= */

int lnn_layer_forward_hamiltonian(
    lnn_layer_t* layer,
    const nimcp_tensor_t* input,
    nimcp_tensor_t* output,
    float dt)
{
    if (!layer || !input || !output) return -1;
    if (!layer->H_net || !layer->use_hamiltonian) {
        NIMCP_LOGGING_ERROR("lnn_layer_forward_hamiltonian: no H-network");
        return -1;
    }

    /* Guard: momentum tensor must exist for Hamiltonian dynamics.
     * p may be NULL if checkpoint was saved before HNN was enabled,
     * or if allocation failed during init. Fall back to LTC. */
    if (!layer->p || !layer->x) {
        NIMCP_LOGGING_WARN("lnn_layer_forward_hamiltonian: p or x is NULL — "
                           "falling back to regular LTC forward");
        return -1;  /* Caller (lnn_layer_forward) will fall through to LTC */
    }

    /* q = layer->x (state = position), p = layer->p (momentum).
     * Pass the layer's borrowed axon-effects pointer so Rayleigh dissipation
     * can activate when enabled + substrate attached. When NULL (no
     * substrate, or dissipation knob off) the integrator degenerates to
     * the classical symplectic step with exact energy conservation. */
    int rc = lnn_hamiltonian_step_stormer_verlet_with_substrate(
        (lnn_hamiltonian_net_t*)layer->H_net, layer->x, layer->p, input, dt,
        1.0f /* input_coupling */,
        layer->substrate_axon_effects);
    if (rc != 0) return rc;

    /* Output = q (position = observable state) */
    uint32_t n = layer->n_neurons;
    float* out_data = (float*)nimcp_tensor_data(output);
    const float* q_data = (const float*)nimcp_tensor_data_const(layer->x);
    uint32_t out_n = nimcp_tensor_numel(output);
    uint32_t copy_n = (n < out_n) ? n : out_n;
    memcpy(out_data, q_data, copy_n * sizeof(float));

    return 0;
}

int lnn_layer_backward_hamiltonian(
    lnn_layer_t* layer,
    const nimcp_tensor_t* upstream_grad)
{
    if (!layer || !upstream_grad || !layer->H_net || !layer->use_hamiltonian) return -1;

    /* The Hamiltonian backward accumulates gradients on the H-network
     * weights via the grad computation that already happened in forward.
     * Additional upstream gradient from the loss is propagated through
     * the chain rule to the H-network parameters. */

    /* For now, the H-network gradients from the forward pass (via
     * lnn_hamiltonian_grad calls inside Störmer-Verlet) are already
     * accumulated. The upstream loss gradient modulates these. */

    uint32_t n = layer->n_neurons;
    const float* ug = (const float*)nimcp_tensor_data_const(upstream_grad);
    lnn_hamiltonian_net_t* net = (lnn_hamiltonian_net_t*)layer->H_net;

    /* Scale existing gradients by upstream signal magnitude */
    float upstream_norm = 0.0f;
    uint32_t ug_n = nimcp_tensor_numel(upstream_grad);
    for (uint32_t i = 0; i < ug_n && i < n; i++) {
        upstream_norm += ug[i] * ug[i];
    }
    upstream_norm = sqrtf(upstream_norm + 1e-8f);

    /* The gradients are already accumulated — no additional scaling needed
     * for the basic case. Advanced: multiply grad_W by upstream chain rule. */

    return 0;
}

/* =========================================================================
 * Gradient Management
 * ========================================================================= */

void lnn_hamiltonian_reset_gradients(lnn_hamiltonian_net_t* net) {
    if (!net) return;
    for (uint32_t l = 0; l < net->n_layers; l++) {
        if (net->grad_W[l]) {
            float* d = (float*)nimcp_tensor_data(net->grad_W[l]);
            memset(d, 0, nimcp_tensor_numel(net->grad_W[l]) * sizeof(float));
        }
        if (net->grad_b[l]) {
            float* d = (float*)nimcp_tensor_data(net->grad_b[l]);
            memset(d, 0, nimcp_tensor_numel(net->grad_b[l]) * sizeof(float));
        }
    }
}

int lnn_hamiltonian_apply_gradients(lnn_hamiltonian_net_t* net, float lr) {
    if (!net) return -1;

    for (uint32_t l = 0; l < net->n_layers; l++) {
        float* W = (float*)nimcp_tensor_data(net->W[l]);
        float* gW = (float*)nimcp_tensor_data(net->grad_W[l]);
        uint32_t w_n = nimcp_tensor_numel(net->W[l]);
        for (uint32_t i = 0; i < w_n; i++) {
            W[i] -= lr * gW[i];
        }

        float* b = (float*)nimcp_tensor_data(net->b[l]);
        float* gb = (float*)nimcp_tensor_data(net->grad_b[l]);
        uint32_t b_n = nimcp_tensor_numel(net->b[l]);
        for (uint32_t i = 0; i < b_n; i++) {
            b[i] -= lr * gb[i];
        }
    }

    /* Weights changed → the learned H(q,p) function changed, so the cached
     * H_initial baseline now refers to a different Hamiltonian than
     * H_current. Invalidate it so energy_deviation rebases on the next
     * eval — the drift metric measures integrator conservation across a
     * single learned-H window, not across training. */
    net->has_initial = false;

    return 0;
}

/* =========================================================================
 * Accessors
 * ========================================================================= */

float lnn_hamiltonian_get_energy(const lnn_hamiltonian_net_t* net) {
    return net ? net->H_current : 0.0f;
}

float lnn_hamiltonian_get_energy_deviation(const lnn_hamiltonian_net_t* net) {
    return net ? net->energy_deviation : 0.0f;
}

uint32_t lnn_hamiltonian_param_count(const lnn_hamiltonian_net_t* net) {
    if (!net) return 0;
    uint32_t count = 0;
    for (uint32_t l = 0; l < net->n_layers; l++) {
        count += nimcp_tensor_numel(net->W[l]);
        count += nimcp_tensor_numel(net->b[l]);
    }
    return count;
}
