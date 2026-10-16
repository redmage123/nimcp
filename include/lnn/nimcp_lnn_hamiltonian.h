/**
 * @file nimcp_lnn_hamiltonian.h
 * @brief Hamiltonian Neural Network extension for LNN layers
 *
 * WHAT: Energy-conserving temporal dynamics via learnable Hamiltonian H(q,p)
 * WHY:  Standard LNN ODE dissipates energy (tau decay). Hamiltonian dynamics
 *       conserve energy by construction — no gradient explosion, stable
 *       long-horizon predictions, mathematically equivalent to FEP.
 * HOW:  Learn H(q,p): R^{2n} → R. Derive dynamics from Hamilton's equations:
 *         dq/dt = ∂H/∂p,  dp/dt = -∂H/∂q
 *       Integrate with Störmer-Verlet (symplectic, energy-preserving).
 *
 * MATHEMATICAL BASIS:
 *   H(q,p) = T(p) + V(q)     [separable Hamiltonian]
 *   H(q,p) = MLP([q;p])      [general Hamiltonian]
 *   Energy conservation: dH/dt = 0 (exact for symplectic integrator)
 *   Connection to FEP: H ≡ variational free energy F
 */

#ifndef NIMCP_LNN_HAMILTONIAN_H
#define NIMCP_LNN_HAMILTONIAN_H

#include "lnn/nimcp_lnn_types.h"
#include "utils/tensor/nimcp_tensor.h"
#include "core/substrate/nimcp_substrate_effects.h"  /* axon_substrate_effects_t */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Configuration
 * ========================================================================= */

typedef struct lnn_hamiltonian_config_s {
    uint32_t hidden_dim;          /**< H-network hidden layer size (0 = auto: 2*n_neurons) */
    uint32_t n_hidden_layers;     /**< H-network depth (default: 2) */
    bool separable;               /**< H(q,p) = T(p) + V(q) for cheaper gradients */
    float energy_penalty_weight;  /**< Regularization weight for dH/dt ≈ 0 */
    float input_coupling;         /**< How strongly external input drives momentum */
} lnn_hamiltonian_config_t;

/* =========================================================================
 * H-Network (learnable energy function)
 * ========================================================================= */

typedef struct lnn_hamiltonian_net_s {
    /* Network architecture */
    uint32_t state_dim;           /**< Dimension of q (and p) */
    uint32_t input_dim;           /**< = 2 * state_dim (concatenated [q;p]) */
    uint32_t n_layers;            /**< Total layers (hidden + output) */
    uint32_t* layer_dims;         /**< Dimension of each layer */
    bool separable;               /**< Separable T(p) + V(q) */

    /* Weights and biases: [n_layers] arrays */
    nimcp_tensor_t** W;           /**< Weight matrices */
    nimcp_tensor_t** b;           /**< Bias vectors */
    nimcp_tensor_t** grad_W;      /**< Weight gradients */
    nimcp_tensor_t** grad_b;      /**< Bias gradients */

    /* Forward pass cache */
    nimcp_tensor_t** pre_act;     /**< Pre-activation values (for backward) */
    nimcp_tensor_t** post_act;    /**< Post-activation values */

    /* Energy tracking */
    float H_current;              /**< Last computed H value */
    float H_initial;              /**< H at t=0 (for drift monitoring) */
    bool has_initial;             /**< Whether H_initial has been set */
    float energy_deviation;       /**< |H_current - H_initial| / |H_initial| */
} lnn_hamiltonian_net_t;

/* =========================================================================
 * API
 * ========================================================================= */

/** Set default config values */
void lnn_hamiltonian_config_default(lnn_hamiltonian_config_t* config);

/** Create H-network for given state dimension */
lnn_hamiltonian_net_t* lnn_hamiltonian_net_create(
    uint32_t state_dim,
    const lnn_hamiltonian_config_t* config);

/** Destroy H-network and free all resources */
void lnn_hamiltonian_net_destroy(lnn_hamiltonian_net_t* net);

/** Evaluate H(q,p) — returns scalar energy value */
float lnn_hamiltonian_eval(
    lnn_hamiltonian_net_t* net,
    const nimcp_tensor_t* q,
    const nimcp_tensor_t* p);

/** Compute ∂H/∂q and ∂H/∂p via backprop through H-network.
 *  dH_dq gives -dp/dt (force), dH_dp gives dq/dt (velocity). */
int lnn_hamiltonian_grad(
    lnn_hamiltonian_net_t* net,
    const nimcp_tensor_t* q,
    const nimcp_tensor_t* p,
    nimcp_tensor_t* dH_dq,
    nimcp_tensor_t* dH_dp);

/** Störmer-Verlet symplectic integrator step.
 *  Updates layer->x (position q) and layer->p (momentum p). */
int lnn_hamiltonian_step_stormer_verlet(
    lnn_hamiltonian_net_t* net,
    nimcp_tensor_t* q,
    nimcp_tensor_t* p,
    const nimcp_tensor_t* input,
    float dt,
    float input_coupling);

/** Forward pass for Hamiltonian layer (replaces standard LTC forward) */
int lnn_layer_forward_hamiltonian(
    lnn_layer_t* layer,
    const nimcp_tensor_t* input,
    nimcp_tensor_t* output,
    float dt);

/** Backward pass for Hamiltonian layer */
int lnn_layer_backward_hamiltonian(
    lnn_layer_t* layer,
    const nimcp_tensor_t* upstream_grad);

/** Zero all H-network gradients */
void lnn_hamiltonian_reset_gradients(lnn_hamiltonian_net_t* net);

/** Apply gradients with learning rate (SGD step) */
int lnn_hamiltonian_apply_gradients(lnn_hamiltonian_net_t* net, float lr);

/** Get current energy value */
float lnn_hamiltonian_get_energy(const lnn_hamiltonian_net_t* net);

/** Get energy deviation from initial: |H(t) - H(0)| / |H(0)| */
float lnn_hamiltonian_get_energy_deviation(const lnn_hamiltonian_net_t* net);

/** Get total parameter count */
uint32_t lnn_hamiltonian_param_count(const lnn_hamiltonian_net_t* net);

/* =========================================================================
 * Rayleigh Dissipation (biologically-motivated, substrate-coupled)
 *
 * Conservation is preserved by construction when dissipation is disabled
 * (the default) or when no substrate is supplied. When enabled and an axon
 * substrate effects struct is provided, Hamilton's equation for momentum is
 * augmented with a Rayleigh-style damping term:
 *
 *   dp/dt = -∂H/∂q - γ(substrate) · p,  γ = γ_max · (1 - overall_capacity)
 *
 * At overall_capacity = 1.0 (healthy metabolism) γ = 0 → conservation. Under
 * metabolic stress overall_capacity shrinks and γ grows toward γ_max,
 * relaxing the system toward equilibrium. Off by default.
 * ========================================================================= */

/** Enable/disable Rayleigh dissipation globally. Any nonzero → on, 0 → off. */
void  hnn_tune_set_dissipation_enabled(float v);
/** Set γ_max in [0.0, 10.0]. Out-of-range values are clamped. */
void  hnn_tune_set_dissipation_gamma_max(float v);

/** Current enabled flag (1.0 on, 0.0 off). */
float hnn_tune_get_dissipation_enabled(void);
/** Current γ_max value. */
float hnn_tune_get_dissipation_gamma_max(void);

/**
 * Compute dq/dt and dp/dt from Hamilton's equations (no integration step).
 *   dq/dt = ∂H/∂p,   dp/dt = -∂H/∂q
 * Legacy API — preserves exact energy conservation. Equivalent to calling
 * the substrate-aware variant with axon_eff = NULL.
 */
int lnn_hamiltonian_forward(
    lnn_hamiltonian_net_t* net,
    const nimcp_tensor_t* q,
    const nimcp_tensor_t* p,
    nimcp_tensor_t* dq,
    nimcp_tensor_t* dp);

/**
 * Substrate-aware variant. If dissipation is enabled AND axon_eff is non-NULL,
 * subtracts γ·p from dp (Rayleigh damping). Otherwise behaves identically to
 * lnn_hamiltonian_forward.
 */
int lnn_hamiltonian_forward_with_substrate(
    lnn_hamiltonian_net_t* net,
    const nimcp_tensor_t* q,
    const nimcp_tensor_t* p,
    nimcp_tensor_t* dq,
    nimcp_tensor_t* dp,
    const axon_substrate_effects_t* axon_eff);

/**
 * Substrate-aware Störmer-Verlet integrator. When dissipation is enabled AND
 * axon_eff is non-NULL, applies Rayleigh damping (-γ·p) on every half-step
 * momentum update. When axon_eff is NULL this reduces to the classical
 * symplectic integrator and preserves energy exactly.
 */
int lnn_hamiltonian_step_stormer_verlet_with_substrate(
    lnn_hamiltonian_net_t* net,
    nimcp_tensor_t* q,
    nimcp_tensor_t* p,
    const nimcp_tensor_t* input,
    float dt,
    float input_coupling,
    const axon_substrate_effects_t* axon_eff);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_HAMILTONIAN_H */
