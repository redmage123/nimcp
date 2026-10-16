# NIMCP Differential Equations & PDE Enhancement Checklist

**Document Purpose:** Implementation roadmap for adding advanced numerical methods and partial differential equations to NIMCP for enhanced biological realism and accuracy.

**Status:** Planning Phase
**Priority:** Medium (Optional enhancements for research/realism mode)
**Target:** NIMCP v2.8+

---

## Executive Summary

### Current State
NIMCP uses **discrete-time approximations** (likely Euler method) for:
- Neuron dynamics (LIF, Izhikevich, Hodgkin-Huxley)
- Plasticity rules (STDP, STP)
- Neuromodulator decay
- All temporal dynamics

### Enhancement Goals
1. **Improve accuracy** of neuron simulations (especially stiff systems like H-H)
2. **Add spatial dynamics** for neuromodulators and glial signals
3. **Enable multi-compartment neurons** for realistic dendritic processing
4. **Maintain performance** - keep enhancements optional, default stays fast

---

## Priority 1: Advanced ODE Solvers (HIGH VALUE, LOW COST)

### Enhancement 1.1: Runge-Kutta 4th Order (RK4) for Neuron Dynamics
**Status:** ⬜ Not Started
**Priority:** ⭐⭐⭐ HIGH
**Effort:** 2-3 days
**Value:** 10x better accuracy, 2x slower (acceptable tradeoff)

#### Current Implementation
- **Files:** `src/core/neuron_models/*.c` (LIF, Izhikevich, AdEx, H-H)
- **Method:** Probably Euler: `V(t+dt) = V(t) + dt * dV/dt`
- **Timestep:** Fixed dt (likely 0.1-1.0 ms)

#### Enhancement Specification
Add RK4 integration option:
```c
// New integration method enum
typedef enum {
    INTEGRATION_EULER,      // Current (fast, 1st order accurate)
    INTEGRATION_RK4,        // New (4th order accurate, 2x slower)
    INTEGRATION_ADAPTIVE,   // Future: variable timestep
    INTEGRATION_IMPLICIT    // Future: for stiff systems
} integration_method_t;

// Add to neuron_config_t
typedef struct {
    integration_method_t integration_method;
    float base_timestep;  // dt in milliseconds
    // ... existing fields
} neuron_config_t;
```

#### Implementation Steps
- [ ] **Step 1:** Create `src/utils/numerical/nimcp_integration.h` and `.c`
  - Implement `rk4_step()` function (generic for any ODE)
  - ~200 lines of code
- [ ] **Step 2:** Refactor neuron models to use integration API
  - Modify `nimcp_lif.c`: extract `compute_dV_dt()` function
  - Modify `nimcp_izhikevich.c`: extract `compute_derivatives()`
  - Modify `nimcp_hodgkin_huxley.c`: extract 4 derivative functions
- [ ] **Step 3:** Add configuration option to `brain_config_t`
  ```c
  brain_config_t config = {
      .neuron_integration = INTEGRATION_RK4,  // vs INTEGRATION_EULER
      ...
  };
  ```
- [ ] **Step 4:** Write validation tests
  - Compare Euler vs RK4 for known analytical solutions
  - Test with stiff H-H equations (expect RK4 to be stable)
  - Performance benchmark (expect 2x slowdown)
- [ ] **Step 5:** Update documentation
  - API reference for integration methods
  - Performance tradeoffs section
  - When to use which method

#### Acceptance Criteria
- ✅ RK4 produces 10x lower error than Euler for same timestep
- ✅ H-H neuron stable with RK4 at dt=1.0ms (may be unstable with Euler)
- ✅ Performance degradation ≤ 2.5x for typical networks
- ✅ All existing tests pass with both Euler and RK4
- ✅ Zero breaking changes (Euler remains default)

#### References
- **Files to modify:** `src/core/neuron_models/nimcp_lif.c`, `nimcp_izhikevich.c`, `nimcp_hodgkin_huxley.c`
- **Algorithm:** Standard RK4 (Runge-Kutta 1901)
- **Test location:** `test/unit/test_numerical_integration.cpp`

---

### Enhancement 1.2: Adaptive Timestep Integration (RK45/Dormand-Prince)
**Status:** ⬜ Not Started
**Priority:** ⭐⭐ MEDIUM
**Effort:** 5-7 days (depends on 1.1)
**Value:** Automatic accuracy control, faster for slow dynamics

#### Enhancement Specification
Add adaptive timestep solver that automatically adjusts dt based on error estimate:
```c
typedef struct {
    integration_method_t method;  // INTEGRATION_ADAPTIVE
    float min_timestep;           // e.g., 0.01 ms
    float max_timestep;           // e.g., 10.0 ms
    float error_tolerance;        // e.g., 1e-6
} adaptive_integration_config_t;
```

#### Implementation Steps
- [ ] **Step 1:** Implement Dormand-Prince (RK45) solver
  - 6-stage Runge-Kutta with embedded error estimate
  - ~400 lines of code
- [ ] **Step 2:** Add timestep adaptation logic
  - Compute error estimate: `err = |RK5 - RK4|`
  - Adjust dt: `dt_new = dt * (tol/err)^(1/5)`
- [ ] **Step 3:** Handle events (spikes, input changes)
  - Detect zero-crossings for spike times
  - Reduce dt automatically when inputs change
- [ ] **Step 4:** Integrate with neuron models
  - Works for all ODE-based neurons
- [ ] **Step 5:** Performance optimization
  - Cache intermediate results
  - Vectorize across multiple neurons

#### Acceptance Criteria
- ✅ Maintains error < tolerance for all test cases
- ✅ 3-10x faster than fixed-timestep for slow dynamics
- ✅ Correctly detects spike times to <0.01ms accuracy
- ✅ Gracefully handles stiff systems (reduces dt automatically)

#### References
- **Algorithm:** Dormand-Prince (1980), same as MATLAB's ode45
- **Implementation guide:** Hairer, Nørsett, Wanner (1993) "Solving Ordinary Differential Equations I"

---

### Enhancement 1.3: Implicit Methods for Stiff Systems
**Status:** ⬜ Not Started
**Priority:** ⭐ LOW
**Effort:** 10-14 days
**Value:** Enables large timesteps for H-H, but requires nonlinear solver

#### Enhancement Specification
Add backward Euler or BDF methods for stiff Hodgkin-Huxley equations:
```c
// Requires solving: V(t+dt) = V(t) + dt * f(V(t+dt), t+dt)
// Nonlinear system → need Newton-Raphson or similar
```

#### Implementation Steps
- [ ] **Step 1:** Implement backward Euler
- [ ] **Step 2:** Add Newton-Raphson solver
- [ ] **Step 3:** Compute Jacobian matrices for each neuron model
- [ ] **Step 4:** Benchmark vs explicit methods

#### Acceptance Criteria
- ✅ Stable with dt=10ms for H-H (vs dt=0.01ms for Euler)
- ✅ Useful for slow dynamics (sub-threshold oscillations)

#### Decision: DEFER
**Reason:** Complex, expensive, marginal value for NIMCP's use cases. Only needed if H-H becomes primary neuron model and real-time constraints relaxed.

---

## Priority 2: Spatial Dynamics (PDEs) - Neuromodulation

### Enhancement 2.1: Graph-Based Neuromodulator Diffusion
**Status:** ⬜ Not Started
**Priority:** ⭐⭐⭐ HIGH
**Effort:** 5-7 days
**Value:** Realistic spatial propagation of dopamine, serotonin, etc.

#### Current Implementation
- **Files:** `src/plasticity/neuromodulators/nimcp_neuromodulators.c`
- **Limitation:** Point-based or global neuromodulator levels (no spatial structure)
- **Missing:** Diffusion from release sites to distant neurons

#### Enhancement Specification
Model neuromodulator concentration as a field on the network graph:
```
∂c/∂t = D * (diffusion term) - k*c + S(x,t)
```

Discretized on graph:
```c
// For each neuron i:
//   c_i(t+dt) = c_i(t) + dt * [D * Σ_j (c_j - c_i) - k*c_i + S_i]
//   where j are neighbors of i

typedef struct {
    float *concentration;       // [num_neurons] array
    float diffusion_coeff;      // D (e.g., 0.1 per ms)
    float decay_rate;           // k (e.g., 0.01 per ms)
    float *source_rate;         // S_i [num_neurons], release rate
} spatial_neuromodulator_t;
```

#### Implementation Steps
- [ ] **Step 1:** Extend neuromodulator system structure
  - Add `spatial_neuromodulator_t` to `neuromodulator_system_t`
  - Store concentration per neuron (not global)
- [ ] **Step 2:** Implement diffusion update
  ```c
  void neuromodulator_diffuse_step(
      spatial_neuromodulator_t *nm,
      neural_network_t *net,
      float dt
  );
  ```
  - Iterate over edges in network topology
  - Compute flux between connected neurons
  - Apply decay to all neurons
- [ ] **Step 3:** Integrate with synapse computation
  - Each synapse reads local concentration at post-synaptic neuron
  - Modulates weight based on local DA/5HT/ACh level
- [ ] **Step 4:** Add release mechanism
  ```c
  void neuromodulator_release_at_neuron(
      spatial_neuromodulator_t *nm,
      uint32_t neuron_id,
      float amount
  );
  ```
  - Triggered by reward signals, novelty, etc.
  - Creates spatial gradient
- [ ] **Step 5:** Visualization support
  - Export concentration field for 3D visualization
  - Heatmap overlay on network topology

#### Implementation Details
**Algorithm:** Explicit Euler on graph Laplacian
```c
for (uint32_t i = 0; i < num_neurons; i++) {
    float laplacian = 0.0f;

    // Sum over neighbors
    for (uint32_t j = 0; j < net->neurons[i].num_connections; j++) {
        uint32_t neighbor = net->neurons[i].connections[j];
        laplacian += (nm->concentration[neighbor] - nm->concentration[i]);
    }

    // Update concentration
    float dC_dt = D * laplacian - k * nm->concentration[i] + nm->source_rate[i];
    nm->concentration[i] += dt * dC_dt;
}
```

**Performance:** O(E) where E = number of edges (sparse networks → fast)

#### Acceptance Criteria
- ✅ Dopamine released at neuron A diffuses to nearby neurons over ~100ms
- ✅ Concentration decays exponentially with time constant 1/k
- ✅ Distant neurons receive lower concentrations (spatial gradient)
- ✅ Performance overhead < 10% for typical networks (10K neurons, 100K synapses)
- ✅ Integrates with curiosity module (novelty → DA release → exploration boost)
- ✅ Integrates with ethics module (empathy → 5HT release → prosocial behavior)

#### Use Cases Enabled
1. **Curiosity exploration:** Follow dopamine gradient toward novel regions
2. **Emotional contagion:** Serotonin diffuses during empathy scenarios
3. **Attention gating:** Acetylcholine modulates local processing regions
4. **Reward learning:** Temporal credit assignment via DA diffusion

#### References
- **Files to modify:** `src/plasticity/neuromodulators/nimcp_neuromodulators.c`, `.h`
- **New files:** `src/plasticity/neuromodulators/nimcp_spatial_neuromodulation.c`, `.h`
- **Test location:** `test/unit/test_spatial_neuromodulation.cpp`
- **Biology:** Fuxe & Agnati (1991) "Volume Transmission in the Brain"

---

### Enhancement 2.2: 3D Grid-Based Neuromodulator Diffusion (Advanced)
**Status:** ⬜ Not Started
**Priority:** ⭐ LOW
**Effort:** 14-21 days
**Value:** Full PDE solution, more realistic but expensive

#### Enhancement Specification
Solve diffusion PDE on 3D spatial grid:
```
∂c/∂t = D∇²c - kc + S(x,y,z,t)
```

Finite difference discretization:
```c
// 3D grid with spacing dx, dy, dz
c[i,j,k](t+dt) = c[i,j,k](t) + dt * D * (
    (c[i+1,j,k] - 2*c[i,j,k] + c[i-1,j,k]) / dx² +
    (c[i,j+1,k] - 2*c[i,j,k] + c[i,j-1,k]) / dy² +
    (c[i,j,k+1] - 2*c[i,j,k] + c[i,j,k-1]) / dz²
) - dt * k * c[i,j,k] + dt * S[i,j,k]
```

#### Implementation Steps
- [ ] **Step 1:** Define 3D spatial grid structure
- [ ] **Step 2:** Map neurons to grid locations
- [ ] **Step 3:** Implement finite difference stencil
- [ ] **Step 4:** Optimize with GPU (CUDA kernel)
- [ ] **Step 5:** Interpolate concentrations to neuron positions

#### Decision: DEFER
**Reason:** Graph-based diffusion (2.1) captures 90% of the value at 10% of the cost. Only needed for publications requiring extreme biophysical accuracy.

---

## Priority 3: Multi-Compartment Neurons (Cable Theory)

### Enhancement 3.1: Two-Compartment Neurons (Soma + Dendrite)
**Status:** ⬜ Not Started
**Priority:** ⭐⭐ MEDIUM
**Effort:** 7-10 days
**Value:** Enables realistic synaptic integration, dendritic filtering

#### Current Implementation
- **Files:** `src/core/neuron_models/*.c`
- **Limitation:** Point neurons (single voltage variable V)
- **Missing:** Dendritic compartments, spatiotemporal integration

#### Enhancement Specification
Extend neurons to have 2 compartments with coupling:
```c
typedef struct {
    // Somatic compartment
    float V_soma;           // Membrane potential at soma
    float I_soma;           // Input current to soma

    // Dendritic compartment
    float V_dend;           // Membrane potential at dendrite
    float I_dend;           // Input current to dendrite

    // Coupling parameters
    float g_couple;         // Coupling conductance (nS)
    float C_soma;           // Somatic capacitance (pF)
    float C_dend;           // Dendritic capacitance (pF)
} two_compartment_neuron_t;
```

**Coupled ODEs:**
```
C_soma * dV_soma/dt = g_leak*(E_L - V_soma) + I_soma + g_couple*(V_dend - V_soma)
C_dend * dV_dend/dt = g_leak*(E_L - V_dend) + I_dend + g_couple*(V_soma - V_dend)
```

#### Implementation Steps
- [ ] **Step 1:** Create new neuron type `NEURON_TYPE_TWO_COMPARTMENT`
- [ ] **Step 2:** Implement coupled differential equations
- [ ] **Step 3:** Assign synapses to compartments
  - Proximal synapses → soma
  - Distal synapses → dendrite
- [ ] **Step 4:** Tune coupling strength for realistic filtering
  - Distal inputs should be attenuated 50-80%
- [ ] **Step 5:** Integrate with existing plasticity rules
  - STDP uses somatic spike times
  - Input timing affected by dendritic delays

#### Use Cases
1. **Visual cortex (V1):** Simple cells require dendritic integration
2. **Temporal filtering:** Distal inputs integrate more slowly
3. **Coincidence detection:** Sharp time windows at soma

#### Acceptance Criteria
- ✅ Distal synaptic inputs attenuate by 50-80% vs proximal
- ✅ Dendritic delay ~1-5ms depending on compartment distance
- ✅ Performance: < 2x slower than point neurons
- ✅ Can reproduce V1 simple cell responses

#### References
- **New files:** `src/core/neuron_models/nimcp_two_compartment.c`, `.h`
- **Test location:** `test/unit/test_two_compartment_neuron.cpp`
- **Biology:** Rall (1967) "Distinguishing theoretical synaptic potentials computed for different soma-dendritic distributions"

---

### Enhancement 3.2: Multi-Compartment Cable Equation (Advanced)
**Status:** ⬜ Not Started
**Priority:** ⭐ LOW
**Effort:** 21-30 days
**Value:** Full dendritic morphology, active dendrites

#### Enhancement Specification
Solve cable equation on dendritic tree:
```
λ²(∂²V/∂x²) = τ(∂V/∂t) + V - E_L + R*I_syn(x,t)
```

Where:
- λ = space constant (mm)
- τ = time constant (ms)
- x = position along dendrite

#### Decision: DEFER
**Reason:** Two-compartment model (3.1) captures most value. Full cable theory only needed for detailed morphological studies.

---

## Priority 4: Glial Calcium Waves (PDEs)

### Enhancement 4.1: Reaction-Diffusion Calcium in Astrocytes
**Status:** ⬜ Not Started
**Priority:** ⭐⭐ MEDIUM
**Effort:** 10-14 days
**Value:** Realistic glial signaling, enhances existing astrocyte module

#### Current Implementation
- **Files:** `src/glial/astrocytes/nimcp_astrocytes.c`
- **Limitation:** Astrocyte effects likely point-based
- **Missing:** Calcium wave propagation through astrocyte networks

#### Enhancement Specification
Model intracellular calcium dynamics with diffusion:
```
∂Ca²⁺/∂t = D_Ca∇²Ca²⁺ + J_release - J_uptake
∂IP3/∂t = D_IP3∇²IP3 + production - degradation
```

Graph-based discretization (astrocyte network):
```c
typedef struct {
    float *calcium;         // [num_astrocytes] Ca²⁺ concentration
    float *ip3;             // [num_astrocytes] IP3 concentration
    float D_ca;             // Diffusion coefficient for Ca²⁺
    float D_ip3;            // Diffusion coefficient for IP3
} astrocyte_calcium_system_t;
```

#### Implementation Steps
- [ ] **Step 1:** Extend astrocyte structure with calcium dynamics
- [ ] **Step 2:** Implement coupled reaction-diffusion equations
  - Ca²⁺ induces IP3 release (positive feedback)
  - Ca²⁺ uptake by ER
  - IP3 diffuses to neighbors
- [ ] **Step 3:** Trigger calcium waves
  - Neuronal activity → glutamate release
  - Glutamate → astrocyte calcium spike
  - Calcium propagates to neighboring astrocytes
- [ ] **Step 4:** Astrocyte → neuron feedback
  - Elevated Ca²⁺ → gliotransmitter release (glutamate, ATP, D-serine)
  - Modulate nearby synapses
- [ ] **Step 5:** Integrate with existing glial integration module
  - Enhance `src/glial/integration/nimcp_glial_integration.c`

#### Use Cases
1. **Sleep consolidation:** Calcium waves during slow-wave sleep
2. **Metabolic coordination:** Astrocytes coordinate energy delivery
3. **Synaptic plasticity:** Gliotransmitters modulate LTP/LTD

#### Acceptance Criteria
- ✅ Calcium waves propagate at ~10-20 μm/s (realistic speed)
- ✅ Wave triggers gliotransmitter release modulating nearby synapses
- ✅ Integrates with existing astrocyte tripartite synapse code
- ✅ Performance overhead < 15%

#### References
- **Files to modify:** `src/glial/astrocytes/nimcp_astrocytes.c`
- **New files:** `src/glial/astrocytes/nimcp_calcium_waves.c`, `.h`
- **Biology:** Cornell-Bell et al. (1990) "Glutamate induces calcium waves in cultured astrocytes"

---

## Priority 5: Field Effects (Ephaptic Coupling)

### Enhancement 5.1: Local Field Potential Computation
**Status:** ⬜ Not Started
**Priority:** ⭐ LOW
**Effort:** 14-21 days
**Value:** Explains some synchronization, enhances oscillations module

#### Enhancement Specification
Compute extracellular potential field from neuron currents:
```
∇²Φ = -σ⁻¹ Σ I_membrane(x,t)
```

Simplified line-source approximation:
```c
// Each neuron contributes to local field
for each neuron i:
    for each nearby neuron j (within radius R):
        distance = |r_i - r_j|
        field_contribution = I_membrane[i] / (4π σ distance)
        Phi[j] += field_contribution
```

#### Implementation Steps
- [ ] **Step 1:** Assign 3D positions to neurons
- [ ] **Step 2:** Compute pairwise distances (spatial indexing for efficiency)
- [ ] **Step 3:** Calculate field potential at each neuron
- [ ] **Step 4:** Feedback: field potential → small membrane current
  - Weak effect (~1% of synaptic input)
  - Can synchronize nearby neurons

#### Decision: DEFER
**Reason:** Small effect, expensive computation (O(N²) or O(N log N) with spatial indexing). Only relevant for large-scale oscillation studies.

---

## Implementation Roadmap

### Phase 1: Foundation (Weeks 1-2)
- ✅ **Enhancement 1.1:** RK4 integration for neuron models
  - Most value, least cost
  - Improves all neuron simulations

### Phase 2: Spatial Neuromodulation (Weeks 3-4)
- ✅ **Enhancement 2.1:** Graph-based neuromodulator diffusion
  - High value for cognitive modules
  - Enables spatial cognition effects

### Phase 3: Advanced Neurons (Weeks 5-6)
- ✅ **Enhancement 3.1:** Two-compartment neurons
  - Important for visual cortex realism
  - Enables dendritic filtering

### Phase 4: Glial Enhancement (Weeks 7-8)
- ✅ **Enhancement 4.1:** Astrocyte calcium waves
  - Completes glial system
  - Research value

### Phase 5: Optional Advanced (Future)
- ⏸️ **Enhancement 1.2:** Adaptive timestep (if needed)
- ⏸️ **Enhancement 1.3:** Implicit methods (if stiff systems become primary)
- ⏸️ **Enhancement 3.2:** Full cable theory (if morphology studies needed)
- ⏸️ **Enhancement 5.1:** Field effects (if oscillation research focus)

---

## Configuration Strategy

### Design Principle: Backward Compatibility
All enhancements are **optional** and **off by default**:

```c
typedef struct {
    // Integration methods (Enhancement 1.x)
    integration_method_t neuron_integration;  // Default: INTEGRATION_EULER

    // Spatial neuromodulation (Enhancement 2.x)
    bool enable_spatial_neuromod;             // Default: false
    float neuromod_diffusion_coeff;           // Default: 0.1
    float neuromod_decay_rate;                // Default: 0.01

    // Multi-compartment neurons (Enhancement 3.x)
    bool enable_multicompartment;             // Default: false
    uint32_t num_compartments;                // Default: 1 (point neuron)

    // Glial calcium waves (Enhancement 4.x)
    bool enable_calcium_waves;                // Default: false
    float calcium_diffusion_coeff;            // Default: 0.05

    // ... existing config fields
} brain_config_t;
```

### Usage Modes

**Mode 1: Fast Inference (Default)**
```c
brain_config_t config = brain_config_default();
// Everything uses fast discrete methods
// Same as current NIMCP
```

**Mode 2: Accurate Simulation**
```c
brain_config_t config = brain_config_default();
config.neuron_integration = INTEGRATION_RK4;
config.enable_spatial_neuromod = true;
// 2-3x slower, much more accurate
```

**Mode 3: Full Biophysical Realism**
```c
brain_config_t config = brain_config_default();
config.neuron_integration = INTEGRATION_RK4;
config.enable_spatial_neuromod = true;
config.enable_multicompartment = true;
config.enable_calcium_waves = true;
// 5-10x slower, publication-quality accuracy
```

---

## Testing Strategy

### For Each Enhancement:

1. **Unit Tests**
   - Test numerical accuracy vs analytical solutions
   - Test edge cases (zero diffusion, zero decay, etc.)
   - Test performance (measure overhead)

2. **Integration Tests**
   - Test with full brain system
   - Verify no breaking changes to existing code
   - Test with all existing cognitive modules

3. **Validation Tests**
   - Compare against known neuroscience results
   - Reproduce published experiments
   - Verify biological realism

4. **Regression Tests**
   - All existing tests must pass with enhancements disabled
   - Zero impact on default configuration

---

## Performance Budget

### Target Performance Impact (vs current baseline):

| Enhancement | Overhead (Typical Network) |
|-------------|---------------------------|
| RK4 integration | +100% (2x slower) |
| Spatial neuromodulation | +10% |
| Two-compartment neurons | +100% (2x slower) |
| Calcium waves | +15% |
| **All enabled** | **~5x slower** |

**Acceptable:** Research/simulation mode can be 5-10x slower
**Unacceptable:** Default mode must not slow down (all enhancements OFF by default)

---

## Success Metrics

### Technical Metrics
- [ ] RK4 achieves 10x lower integration error than Euler
- [ ] Spatial neuromodulation creates realistic gradients (verified by visualization)
- [ ] Two-compartment neurons reproduce V1 simple cell responses
- [ ] Calcium waves propagate at realistic speeds (10-20 μm/s)
- [ ] All enhancements have <30% performance overhead individually

### Scientific Metrics
- [ ] Can reproduce at least 3 published neuroscience experiments
- [ ] Enables new research directions (spatial cognition, dendritic computation)
- [ ] Results publishable in computational neuroscience journals

### Engineering Metrics
- [ ] Zero breaking changes to existing API
- [ ] All existing tests pass with enhancements disabled
- [ ] Code coverage >85% for new code
- [ ] Documentation complete for all new features

---

## Related Work & References

### Numerical Methods
- Hairer, Nørsett, Wanner (1993) "Solving Ordinary Differential Equations I & II"
- Press et al. (2007) "Numerical Recipes" (RK4, adaptive methods)

### Neuroscience
- Rall (1967) - Cable theory, dendritic integration
- Cornell-Bell et al. (1990) - Astrocyte calcium waves
- Fuxe & Agnati (1991) - Volume transmission, neuromodulator diffusion
- Koch (1999) "Biophysics of Computation" - Comprehensive reference

### Software
- **SUNDIALS** (sundials.readthedocs.io) - Advanced ODE/DAE solvers
- **NEURON** (neuron.yale.edu) - Multi-compartment neuron simulator (inspiration)
- **Brian2** (briansimulator.org) - Spiking neural network simulator with PDEs

---

## Notes & Decisions

### Key Design Decisions
1. **All enhancements optional:** Maintains backward compatibility and performance
2. **Graph-based PDEs:** Use network topology instead of 3D grid (faster, good enough)
3. **RK4 before adaptive:** Simpler, captures most value
4. **Defer field effects:** Low priority, high cost

### Questions for Discussion
- [ ] Should we add SUNDIALS as dependency for advanced ODE solving?
- [ ] GPU acceleration for spatial diffusion? (CUDA kernels)
- [ ] Export spatial fields for visualization? (VTK format?)
- [ ] Target any specific neuroscience experiments to reproduce?

---

## Tracking

**Document Version:** 1.0
**Last Updated:** 2025-11-11
**Status:** Ready for Implementation
**Next Review:** After Phase 1 completion

**Progress Tracking:**
- Phase 1: ⬜ 0% (Not Started)
- Phase 2: ⬜ 0% (Not Started)
- Phase 3: ⬜ 0% (Not Started)
- Phase 4: ⬜ 0% (Not Started)

---

## Quick Reference: Implementation Order

**If implementing sequentially:**
1. Enhancement 1.1 - RK4 (biggest bang for buck)
2. Enhancement 2.1 - Spatial neuromodulation (enables spatial cognition)
3. Enhancement 3.1 - Two-compartment neurons (visual cortex)
4. Enhancement 4.1 - Calcium waves (completes glial system)

**If time-limited, implement only:**
- Enhancement 1.1 (RK4) - Improves everything, minimal cost

**If research-focused:**
- Enhancement 2.1 + Enhancement 4.1 - Novel spatial dynamics for publications
