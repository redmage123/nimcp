# Hamiltonian Neural Networks (HNN)

## Overview

Energy-conserving temporal dynamics for LNN layers via learnable Hamiltonian H(q,p).

**Key Insight**: H(q,p) = variational free energy F. Hamilton's equations give optimal inference dynamics. The symplectic integrator preserves energy by construction — no gradient explosion.

## Architecture

```
Standard LNN:    dx/dt = -x/τ + f(W·input + W_rec·x)    [dissipative]
Hamiltonian LNN: dq/dt = ∂H/∂p,  dp/dt = -∂H/∂q        [conservative]
```

### H-Network (learnable energy function)
- Input: [q; p] concatenated (2 × n_neurons)
- Hidden: 2 layers, softplus activation (smooth, positive)
- Output: scalar H (total energy)
- Gradients ∂H/∂q and ∂H/∂p computed via backprop → give equations of motion

### Störmer-Verlet Integrator (symplectic)
```
1. p_half = p - (dt/2) * ∂H/∂q(q, p)
2. q_new  = q + dt * ∂H/∂p(q, p_half)
3. p_new  = p_half - (dt/2) * ∂H/∂q(q_new, p_half)
```
Energy deviation |H(t) - H(0)| / |H(0)| stays near machine epsilon.

## Files

| File | Purpose |
|------|---------|
| `include/lnn/nimcp_lnn_hamiltonian.h` | Types, config, API |
| `src/lnn/nimcp_lnn_hamiltonian.c` | H-network, integrator, forward/backward |
| `src/cognitive/free_energy/nimcp_fep_hnn_fno_bridges.c` | HNN→FEP bridge |

## API

```c
// Configuration
lnn_hamiltonian_config_t cfg;
lnn_hamiltonian_config_default(&cfg);
cfg.hidden_dim = 128;         // H-network hidden size
cfg.n_hidden_layers = 2;       // H-network depth
cfg.separable = false;         // General H(q,p)

// Create H-network
lnn_hamiltonian_net_t* net = lnn_hamiltonian_net_create(state_dim, &cfg);

// Evaluate energy
float H = lnn_hamiltonian_eval(net, q, p);

// Compute gradients (Hamilton's equations)
lnn_hamiltonian_grad(net, q, p, dH_dq, dH_dp);

// Symplectic step
lnn_hamiltonian_step_stormer_verlet(net, q, p, input, dt, coupling);

// Monitor energy conservation
float deviation = lnn_hamiltonian_get_energy_deviation(net);
```

## Enabling on LNN Layer

```c
// In lnn_layer_s struct:
layer->use_hamiltonian = true;
layer->H_net = lnn_hamiltonian_net_create(layer->n_neurons, &cfg);
layer->p = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);  // momentum
```

When `use_hamiltonian` is true, `lnn_layer_forward()` dispatches to `lnn_layer_forward_hamiltonian()`.

## FEP Connection

- H(q,p) IS the variational free energy F
- Energy deviation = prediction error (model fit quality)
- Precision = 1/variance of recent H values
- Registered as FEP_BRIDGE_CATEGORY_CORE in FEP orchestrator

## Training Metrics

Available via `brain.get_network_metrics()`:
- `hnn_energy` — current H(q,p) value
- `hnn_energy_deviation` — drift from initial H
- `hnn_initial_energy` — H at t=0
- `hnn_active` — whether Hamiltonian is active

## Gotchas

1. **Momentum tensor p**: Must be allocated alongside state x. Destroyed in layer cleanup.
2. **Softplus activation**: Required for smooth energy landscape. ReLU causes non-smooth gradients.
3. **Energy conservation test**: After 1000 steps, deviation should be < 0.1. If not, dt is too large.
4. **H-network is NOT the same as the LNN layer** — it's an auxiliary network that defines the energy landscape.
