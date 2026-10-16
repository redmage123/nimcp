# Liquid Neural Network (LNN) Module

Liquid Time-Constant (LTC) neural network with continuous-time dynamics.

## Architecture

- **NCP wiring**: 128 sensory -> 64 inter -> 32 command -> 64 motor (288 neurons)
- ODE-based continuous-time dynamics (neuron membrane potential evolution)
- Adjoint method for gradient computation (memory-efficient backward-in-time)

## LTC Neuron Dynamics
```
dx/dt = -x/tau(x,I) + f(W_in*I + W_rec*x + b)
tau(x,I) = tau_base * sigma(W_tau*[x;I] + b_tau)
```

## Wiring Patterns
- `LNN_WIRING_FULL` - Dense all-to-all
- `LNN_WIRING_RANDOM` - Erdos-Renyi sparse
- `LNN_WIRING_SMALL_WORLD` - Watts-Strogatz
- `LNN_WIRING_SCALE_FREE` - Barabasi-Albert hubs
- `LNN_WIRING_NCP` - Neural Circuit Policy

## ODE Solvers
- `LNN_ODE_EULER` - 1st order
- `LNN_ODE_HEUN` - 2nd order
- `LNN_ODE_RK4` - 4th order (default)
- `LNN_ODE_DOPRI5` - Adaptive 5th order

## Gradient Explosion Fixes (4 fixes)
1. Per-layer tensor clipping (not dead grad_params)
2. Clip called in training loop
3. Per-step clamping [-1e4, 1e4]
4. tau_safe floor 0.01

## Training
- Adam optimizer, MSE loss, gradient_clip_norm=1.0
- LNN training context with plasticity/immune/bio-async integration
- Sleep bridge modulates tau/LR during sleep cycles

## Stats (via `lnn_get_stats()`)
- forward_steps, backward_steps, ode_evaluations
- avg_tau, state_norm, gradient_norm
- nan_count, inf_count

## Python API
- `brain.lnn_get_stats()` - statistics dict
- `brain.lnn_forward_step()` - single forward step
- `brain.lnn_get_state()` - current LNN state

## Hamiltonian Extension

When `layer->use_hamiltonian = true`, LNN dynamics switch from dissipative to energy-conserving:

```
Standard:    dx/dt = -x/τ + f(W·input + W_rec·x)    [dissipative]
Hamiltonian: dq/dt = ∂H/∂p,  dp/dt = -∂H/∂q        [conservative]
```

- H-network: [q;p] → 2-layer softplus → scalar H (energy)
- Störmer-Verlet symplectic integrator preserves energy by construction
- H(q,p) = variational free energy F in FEP terms
- See [hnn.md](hnn.md) for full details

## Key Files
- 18 header files in `include/lnn/` (includes `nimcp_lnn_hamiltonian.h`)
- `nimcp_lnn_gradient.c` (SRP split into 5 part files)
- `nimcp_lnn_training.c`, `nimcp_lnn_network.c`
- `nimcp_lnn_hamiltonian.c` — H-network, Störmer-Verlet integrator

## Bio-async Module ID: 0x0600

## Test Coverage: 84 tests
