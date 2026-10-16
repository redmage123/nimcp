# Liquid Neural Network (LNN) Implementation Plan for NIMCP

## Executive Summary

This document outlines a complete implementation plan for integrating Liquid Neural Networks (LNNs) across 25+ NIMCP modules. LNNs provide continuous-time dynamics with learnable time constants, replacing hand-tuned temporal parameters with emergent behavior.

**Total Scope:**
- 25 modules to integrate
- ~180 new source files
- ~45,000 lines of code
- ~1,200 new tests
- 8 implementation phases

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Core LNN Library](#2-core-lnn-library)
3. [Integration Infrastructure](#3-integration-infrastructure)
4. [Module Integration Phases](#4-module-integration-phases)
5. [Training Layer Integration](#5-training-layer-integration)
6. [Bio-Async Integration](#6-bio-async-integration)
7. [Immune System Integration](#7-immune-system-integration)
8. [Parallelization Strategy](#8-parallelization-strategy)
9. [Testing Strategy](#9-testing-strategy)
10. [File Manifest](#10-file-manifest)

---

## 1. Architecture Overview

### 1.1 LNN Core Concepts

Liquid Neural Networks are characterized by:
- **Continuous-time dynamics**: State evolves via ODEs
- **Liquid Time Constants (LTC)**: Time constants adapt based on input
- **Sparse wiring**: Biologically-inspired connectivity
- **Temporal processing**: Excellent for time-series data

### 1.2 Mathematical Foundation

**LTC Neuron Dynamics:**
```
dx/dt = -[1/τ(x,I)] * x + f(x,I,θ)

where:
  x     = neuron state vector
  τ(x,I) = input-dependent time constant
  I     = input signal
  θ     = learnable parameters
  f     = nonlinear activation function
```

**Liquid Time Constant:**
```
τ(x,I) = τ_base * σ(W_τ * [x; I] + b_τ)

where:
  τ_base = base time constant (learnable)
  σ      = sigmoid function (bounds τ to (0, 2*τ_base))
  W_τ    = time constant weights
  b_τ    = time constant bias
```

**ODE Solution (RK4):**
```
k1 = dt * f(t, x)
k2 = dt * f(t + dt/2, x + k1/2)
k3 = dt * f(t + dt/2, x + k2/2)
k4 = dt * f(t + dt, x + k3)
x_next = x + (k1 + 2*k2 + 2*k3 + k4) / 6
```

### 1.3 Integration Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           NIMCP LNN ARCHITECTURE                             │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                              LNN CORE LIBRARY                                │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │ LTC Neurons │  │ ODE Solvers │  │ Sparse Wire │  │ Gradient Backprop   │ │
│  │             │  │ (RK4/Euler) │  │ Patterns    │  │ (Adjoint Method)    │ │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────────┬──────────┘ │
│         └────────────────┴────────────────┴─────────────────────┘           │
└─────────────────────────────────────────────────────────────────────────────┘
                                       │
           ┌───────────────────────────┼───────────────────────────┐
           ▼                           ▼                           ▼
┌─────────────────────┐    ┌─────────────────────┐    ┌─────────────────────┐
│   LNN BRIDGE LAYER  │    │  LNN TRAINING LAYER │    │   LNN ASYNC LAYER   │
│  ┌───────────────┐  │    │  ┌───────────────┐  │    │  ┌───────────────┐  │
│  │ Module Adapters│  │    │  │ LNN Optimizer │  │    │  │ Bio-Router    │  │
│  │ (25 modules)  │  │    │  │ Integration   │  │    │  │ Integration   │  │
│  └───────────────┘  │    │  └───────────────┘  │    │  └───────────────┘  │
│  ┌───────────────┐  │    │  ┌───────────────┐  │    │  ┌───────────────┐  │
│  │ State Mappers │  │    │  │ Gradient Flow │  │    │  │ Phase Sync    │  │
│  │ (effects ↔ x) │  │    │  │ (adjoint)     │  │    │  │ (oscillation) │  │
│  └───────────────┘  │    │  └───────────────┘  │    │  └───────────────┘  │
└─────────────────────┘    └─────────────────────┘    └─────────────────────┘
           │                           │                           │
           └───────────────────────────┼───────────────────────────┘
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           NIMCP INFRASTRUCTURE                               │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌───────┐│
│  │ Tensor  │  │ Thread  │  │ Memory  │  │ Logging │  │ Immune  │  │ Error ││
│  │ Library │  │ Pool    │  │ Manager │  │ System  │  │ System  │  │ Codes ││
│  └─────────┘  └─────────┘  └─────────┘  └─────────┘  └─────────┘  └───────┘│
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core LNN Library

### 2.1 Directory Structure

```
include/lnn/
├── nimcp_lnn.h                      # Main include (facade)
├── nimcp_lnn_types.h                # Core type definitions
├── nimcp_lnn_config.h               # Configuration structs
├── nimcp_lnn_neuron.h               # LTC neuron API
├── nimcp_lnn_layer.h                # LNN layer API
├── nimcp_lnn_network.h              # Full network API
├── nimcp_lnn_ode.h                  # ODE solver API
├── nimcp_lnn_wiring.h               # Sparse wiring patterns
├── nimcp_lnn_gradient.h             # Adjoint gradient computation
├── nimcp_lnn_training.h             # Training integration
├── nimcp_lnn_bio_async.h            # Bio-async integration
├── nimcp_lnn_immune.h               # Immune integration
└── nimcp_lnn_parallel.h             # Parallelization API

src/lnn/
├── nimcp_lnn_neuron.c
├── nimcp_lnn_layer.c
├── nimcp_lnn_network.c
├── nimcp_lnn_ode.c
├── nimcp_lnn_wiring.c
├── nimcp_lnn_gradient.c
├── nimcp_lnn_training.c
├── nimcp_lnn_bio_async.c
├── nimcp_lnn_immune.c
└── nimcp_lnn_parallel.c
```

### 2.2 Core Types (nimcp_lnn_types.h)

```c
/**
 * @file nimcp_lnn_types.h
 * @brief Core type definitions for Liquid Neural Networks
 *
 * WHAT: Defines fundamental LNN data structures
 * WHY:  LNNs require continuous-time state and learnable time constants
 * HOW:  Struct definitions with biological grounding
 */

#ifndef NIMCP_LNN_TYPES_H
#define NIMCP_LNN_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/tensor/nimcp_tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

typedef struct lnn_neuron_s lnn_neuron_t;
typedef struct lnn_layer_s lnn_layer_t;
typedef struct lnn_network_s lnn_network_t;
typedef struct lnn_state_s lnn_state_t;
typedef struct lnn_config_s lnn_config_t;
typedef struct lnn_wiring_s lnn_wiring_t;
typedef struct lnn_gradient_ctx_s lnn_gradient_ctx_t;

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief ODE solver methods
 *
 * WHAT: Numerical integration methods for continuous dynamics
 * WHY:  Different methods trade accuracy vs speed
 */
typedef enum {
    LNN_ODE_EULER = 0,          /**< 1st order, fast, less accurate */
    LNN_ODE_HEUN,               /**< 2nd order predictor-corrector */
    LNN_ODE_RK4,                /**< 4th order Runge-Kutta (default) */
    LNN_ODE_DOPRI5,             /**< Adaptive 5th order */
    LNN_ODE_IMPLICIT_EULER,     /**< For stiff systems */
    LNN_ODE_METHOD_COUNT
} lnn_ode_method_t;

/**
 * @brief Activation functions for LNN neurons
 */
typedef enum {
    LNN_ACTIVATION_TANH = 0,    /**< Default, bounded [-1, 1] */
    LNN_ACTIVATION_SIGMOID,     /**< Bounded [0, 1] */
    LNN_ACTIVATION_RELU,        /**< Unbounded, sparse */
    LNN_ACTIVATION_GELU,        /**< Smooth ReLU */
    LNN_ACTIVATION_SILU,        /**< Sigmoid-weighted linear */
    LNN_ACTIVATION_SOFTPLUS,    /**< Smooth ReLU, always positive */
    LNN_ACTIVATION_CUSTOM,      /**< User-defined */
    LNN_ACTIVATION_COUNT
} lnn_activation_t;

/**
 * @brief Wiring patterns for sparse connectivity
 *
 * WHAT: Predefined connectivity patterns inspired by biology
 * WHY:  Sparse wiring reduces parameters and improves generalization
 */
typedef enum {
    LNN_WIRING_FULL = 0,        /**< Dense connectivity (baseline) */
    LNN_WIRING_RANDOM,          /**< Random sparse (Erdos-Renyi) */
    LNN_WIRING_SMALL_WORLD,     /**< Watts-Strogatz small-world */
    LNN_WIRING_SCALE_FREE,      /**< Barabasi-Albert scale-free */
    LNN_WIRING_MODULAR,         /**< Clustered modules with sparse inter */
    LNN_WIRING_FEEDFORWARD,     /**< Strictly feedforward layers */
    LNN_WIRING_RECURRENT,       /**< Full recurrence within layer */
    LNN_WIRING_NCP,             /**< Neural Circuit Policy (original LNN) */
    LNN_WIRING_CUSTOM,          /**< User-defined adjacency */
    LNN_WIRING_COUNT
} lnn_wiring_type_t;

/**
 * @brief Training modes for LNN gradient computation
 */
typedef enum {
    LNN_TRAIN_BPTT = 0,         /**< Backprop through time (memory intensive) */
    LNN_TRAIN_ADJOINT,          /**< Adjoint method (memory efficient) */
    LNN_TRAIN_RTRL,             /**< Real-time recurrent learning */
    LNN_TRAIN_EPROP,            /**< Eligibility propagation (online) */
    LNN_TRAIN_MODE_COUNT
} lnn_train_mode_t;

/**
 * @brief LNN neuron roles (for NCP wiring)
 */
typedef enum {
    LNN_ROLE_SENSORY = 0,       /**< Input neurons */
    LNN_ROLE_INTER,             /**< Hidden interneurons */
    LNN_ROLE_COMMAND,           /**< Decision/output neurons */
    LNN_ROLE_MOTOR,             /**< Output neurons */
    LNN_ROLE_COUNT
} lnn_neuron_role_t;

/**
 * @brief LNN state validity flags
 */
typedef enum {
    LNN_STATE_VALID = 0,
    LNN_STATE_NAN_DETECTED,
    LNN_STATE_INF_DETECTED,
    LNN_STATE_EXPLOSION,
    LNN_STATE_VANISHING,
    LNN_STATE_UNSTABLE
} lnn_state_health_t;

/*=============================================================================
 * Core Structures
 *===========================================================================*/

/**
 * @brief Single LTC neuron state and parameters
 *
 * WHAT: Represents one Liquid Time-Constant neuron
 * WHY:  LTC neurons have input-dependent time constants
 * HOW:  State x evolves via dx/dt = -x/τ(x,I) + f(x,I)
 */
struct lnn_neuron_s {
    /* Neuron identity */
    uint32_t id;
    lnn_neuron_role_t role;

    /* Current state */
    float x;                    /**< Membrane potential / activation */
    float x_prev;               /**< Previous state (for derivatives) */
    float dx_dt;                /**< Current derivative */

    /* Time constant (learnable) */
    float tau_base;             /**< Base time constant [1, 1000] ms */
    float tau_current;          /**< Current effective τ after modulation */
    float* w_tau;               /**< Weights for τ modulation [n_inputs] */
    float b_tau;                /**< Bias for τ modulation */

    /* Input weights (learnable) */
    float* w_in;                /**< Input weights [n_inputs] */
    float b_in;                 /**< Input bias */

    /* Recurrent weights (learnable) */
    float* w_rec;               /**< Recurrent weights [n_neurons] */

    /* Activation */
    lnn_activation_t activation;

    /* Dimensions */
    uint32_t n_inputs;
    uint32_t n_recurrent;

    /* Gradients (for training) */
    float* grad_w_in;
    float* grad_w_rec;
    float* grad_w_tau;
    float grad_b_in;
    float grad_b_tau;
    float grad_tau_base;
};

/**
 * @brief LNN layer containing multiple neurons
 */
struct lnn_layer_s {
    /* Layer identity */
    uint32_t id;
    char name[64];

    /* Neurons */
    lnn_neuron_t* neurons;
    uint32_t n_neurons;

    /* Connectivity */
    lnn_wiring_t* wiring;

    /* Layer-level state (tensor form for SIMD) */
    nimcp_tensor_t* x;          /**< State tensor [n_neurons] */
    nimcp_tensor_t* tau;        /**< Time constants [n_neurons] */
    nimcp_tensor_t* dx_dt;      /**< Derivatives [n_neurons] */

    /* Weight tensors */
    nimcp_tensor_t* W_in;       /**< Input weights [n_neurons, n_inputs] */
    nimcp_tensor_t* W_rec;      /**< Recurrent weights [n_neurons, n_neurons] */
    nimcp_tensor_t* W_tau;      /**< τ modulation weights [n_neurons, n_inputs + n_neurons] */
    nimcp_tensor_t* b_in;       /**< Input bias [n_neurons] */
    nimcp_tensor_t* b_tau;      /**< τ bias [n_neurons] */
    nimcp_tensor_t* tau_base;   /**< Base time constants [n_neurons] */

    /* Gradient tensors */
    nimcp_tensor_t* grad_W_in;
    nimcp_tensor_t* grad_W_rec;
    nimcp_tensor_t* grad_W_tau;
    nimcp_tensor_t* grad_b_in;
    nimcp_tensor_t* grad_b_tau;
    nimcp_tensor_t* grad_tau_base;

    /* ODE solver state */
    lnn_ode_method_t ode_method;
    float dt;                   /**< Time step (ms) */

    /* Statistics */
    uint64_t step_count;
    float avg_tau;
    float min_tau;
    float max_tau;
};

/**
 * @brief Full LNN network with multiple layers
 */
struct lnn_network_s {
    /* Network identity */
    uint32_t id;
    char name[64];

    /* Layers */
    lnn_layer_t** layers;
    uint32_t n_layers;

    /* Input/output dimensions */
    uint32_t n_inputs;
    uint32_t n_outputs;

    /* Global configuration */
    lnn_config_t* config;

    /* Training context */
    lnn_gradient_ctx_t* grad_ctx;
    lnn_train_mode_t train_mode;
    bool is_training;

    /* State history (for BPTT) */
    nimcp_tensor_t** state_history;
    uint32_t history_len;
    uint32_t history_capacity;

    /* Integration handles */
    void* optimizer;            /**< nimcp_optimizer_context_t* */
    void* gradient_manager;     /**< nimcp_gradient_manager_ctx_t* */
    void* bio_ctx;              /**< bio_module_context_t */
    void* immune_bridge;        /**< lnn_immune_bridge_t* */

    /* Thread pool for parallel computation */
    void* thread_pool;          /**< nimcp_thread_pool_t* */
    uint32_t n_threads;

    /* Mutex for thread safety */
    void* mutex;

    /* Statistics */
    lnn_network_stats_t stats;
};

/**
 * @brief Network-level statistics
 */
typedef struct {
    uint64_t forward_steps;
    uint64_t backward_steps;
    uint64_t ode_evaluations;
    double total_compute_time_ms;
    double avg_step_time_ms;
    float avg_tau_network;
    float state_norm;
    float gradient_norm;
    lnn_state_health_t health;
    uint32_t nan_count;
    uint32_t inf_count;
    size_t memory_usage_bytes;
} lnn_network_stats_t;

/**
 * @brief Sparse wiring configuration
 */
struct lnn_wiring_s {
    lnn_wiring_type_t type;

    /* Adjacency representation */
    uint32_t* row_ptr;          /**< CSR row pointers [n_neurons + 1] */
    uint32_t* col_idx;          /**< CSR column indices [n_edges] */
    float* edge_weights;        /**< Optional edge weights [n_edges] */

    uint32_t n_neurons;
    uint32_t n_edges;
    float sparsity;             /**< Fraction of zero connections */

    /* NCP-specific */
    uint32_t n_sensory;
    uint32_t n_inter;
    uint32_t n_command;
    uint32_t n_motor;

    /* Small-world parameters */
    float rewire_prob;          /**< Watts-Strogatz rewiring probability */
    uint32_t k_neighbors;       /**< Initial ring lattice neighbors */

    /* Scale-free parameters */
    uint32_t m_edges;           /**< Edges per new node (Barabasi-Albert) */
};

/**
 * @brief Gradient computation context (adjoint method)
 */
struct lnn_gradient_ctx_s {
    /* Adjoint state */
    nimcp_tensor_t* adjoint;    /**< λ(t) adjoint variables */
    nimcp_tensor_t* adjoint_prev;

    /* Accumulated gradients */
    nimcp_tensor_t* grad_params; /**< Total parameter gradients */

    /* Time integration */
    float t_start;
    float t_end;
    float dt;
    uint32_t n_steps;

    /* Loss gradient */
    nimcp_tensor_t* dL_dx_final; /**< ∂L/∂x at final time */

    /* Checkpointing for memory efficiency */
    bool use_checkpointing;
    uint32_t checkpoint_interval;
    nimcp_tensor_t** checkpoints;
    uint32_t n_checkpoints;
};

/*=============================================================================
 * Error Codes
 *===========================================================================*/

#define LNN_SUCCESS                     0
#define LNN_ERROR_NULL_POINTER         -1
#define LNN_ERROR_INVALID_CONFIG       -2
#define LNN_ERROR_INVALID_DIMENSION    -3
#define LNN_ERROR_OUT_OF_MEMORY        -4
#define LNN_ERROR_INVALID_STATE        -5
#define LNN_ERROR_ODE_DIVERGENCE       -6
#define LNN_ERROR_GRADIENT_EXPLOSION   -7
#define LNN_ERROR_GRADIENT_VANISHING   -8
#define LNN_ERROR_WIRING_INVALID       -9
#define LNN_ERROR_NOT_INITIALIZED     -10
#define LNN_ERROR_THREAD_FAILURE      -11

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_TYPES_H */
```

### 2.3 Configuration (nimcp_lnn_config.h)

```c
/**
 * @file nimcp_lnn_config.h
 * @brief Configuration structures for LNN networks
 */

#ifndef NIMCP_LNN_CONFIG_H
#define NIMCP_LNN_CONFIG_H

#include "nimcp_lnn_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Neuron-level configuration
 */
typedef struct {
    lnn_activation_t activation;
    float tau_base_init;        /**< Initial base τ (ms) */
    float tau_min;              /**< Minimum τ (ms) */
    float tau_max;              /**< Maximum τ (ms) */
    float weight_init_std;      /**< Weight initialization std */
    bool learn_tau;             /**< Whether to learn τ parameters */
} lnn_neuron_config_t;

/**
 * @brief Layer-level configuration
 */
typedef struct {
    uint32_t n_neurons;
    lnn_neuron_config_t neuron_config;
    lnn_wiring_type_t wiring_type;
    float sparsity;             /**< Target sparsity [0, 1) */
    lnn_ode_method_t ode_method;
    float dt;                   /**< Integration time step (ms) */
    bool use_layer_norm;
} lnn_layer_config_t;

/**
 * @brief Network-level configuration
 */
struct lnn_config_s {
    /* Architecture */
    uint32_t n_layers;
    lnn_layer_config_t* layer_configs;
    uint32_t n_inputs;
    uint32_t n_outputs;

    /* ODE settings */
    lnn_ode_method_t default_ode_method;
    float default_dt;
    float adaptive_dt_min;
    float adaptive_dt_max;
    float adaptive_error_tol;

    /* Training */
    lnn_train_mode_t train_mode;
    uint32_t bptt_truncation;   /**< Truncation length for BPTT */
    bool use_gradient_checkpointing;
    uint32_t checkpoint_interval;
    float gradient_clip_norm;

    /* Wiring (NCP) */
    uint32_t ncp_sensory;
    uint32_t ncp_inter;
    uint32_t ncp_command;
    uint32_t ncp_motor;

    /* Parallelization */
    uint32_t n_threads;
    bool enable_simd;

    /* Bio-async */
    bool enable_bio_async;
    uint16_t bio_module_id;

    /* Immune */
    bool enable_immune_integration;
    float instability_threshold;

    /* Logging */
    bool enable_logging;
    int log_level;

    /* Memory */
    size_t max_memory_bytes;
    bool preallocate_history;
};

/**
 * @brief Initialize default configuration
 */
int lnn_config_default(lnn_config_t* config);

/**
 * @brief Create NCP (Neural Circuit Policy) configuration
 *
 * @param n_inputs Number of input neurons
 * @param n_inter Number of interneurons
 * @param n_command Number of command neurons
 * @param n_outputs Number of output neurons
 */
int lnn_config_ncp(lnn_config_t* config,
                   uint32_t n_inputs,
                   uint32_t n_inter,
                   uint32_t n_command,
                   uint32_t n_outputs);

/**
 * @brief Validate configuration
 */
int lnn_config_validate(const lnn_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_CONFIG_H */
```

### 2.4 Core API (nimcp_lnn.h)

```c
/**
 * @file nimcp_lnn.h
 * @brief Main include file for NIMCP Liquid Neural Network library
 *
 * WHAT: Provides continuous-time neural networks with learnable time constants
 * WHY:  Enables biologically-plausible temporal dynamics in NIMCP modules
 * HOW:  LTC neurons with ODE-based state evolution and sparse wiring
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#ifndef NIMCP_LNN_H
#define NIMCP_LNN_H

/* Include all LNN headers */
#include "nimcp_lnn_types.h"
#include "nimcp_lnn_config.h"
#include "nimcp_lnn_neuron.h"
#include "nimcp_lnn_layer.h"
#include "nimcp_lnn_network.h"
#include "nimcp_lnn_ode.h"
#include "nimcp_lnn_wiring.h"
#include "nimcp_lnn_gradient.h"
#include "nimcp_lnn_training.h"
#include "nimcp_lnn_bio_async.h"
#include "nimcp_lnn_immune.h"
#include "nimcp_lnn_parallel.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Library Lifecycle
 *===========================================================================*/

/**
 * @brief Initialize LNN library
 *
 * WHAT: Initialize global LNN state and thread pool
 * WHY:  Required before creating any LNN networks
 * HOW:  Allocate thread pool, initialize SIMD, register bio-async
 *
 * @param n_threads Number of worker threads (0 = auto-detect)
 * @return 0 on success, negative on error
 */
int lnn_init(uint32_t n_threads);

/**
 * @brief Shutdown LNN library
 */
void lnn_shutdown(void);

/**
 * @brief Get library version
 */
const char* lnn_version(void);

/*=============================================================================
 * Network Lifecycle
 *===========================================================================*/

/**
 * @brief Create LNN network from configuration
 */
lnn_network_t* lnn_network_create(const lnn_config_t* config);

/**
 * @brief Create NCP network (convenience function)
 */
lnn_network_t* lnn_network_create_ncp(
    uint32_t n_inputs,
    uint32_t n_inter,
    uint32_t n_command,
    uint32_t n_outputs
);

/**
 * @brief Destroy network and free resources
 */
void lnn_network_destroy(lnn_network_t* network);

/*=============================================================================
 * Forward Pass
 *===========================================================================*/

/**
 * @brief Single forward step with given input
 *
 * WHAT: Advance network state by one time step
 * WHY:  Core inference operation
 * HOW:  Compute input→state, update τ, solve ODE
 *
 * @param network LNN network
 * @param input Input tensor [n_inputs]
 * @param output Output tensor [n_outputs] (filled by function)
 * @param dt Time step (0 = use network default)
 * @return 0 on success
 */
int lnn_forward_step(
    lnn_network_t* network,
    const nimcp_tensor_t* input,
    nimcp_tensor_t* output,
    float dt
);

/**
 * @brief Forward pass over sequence
 *
 * @param network LNN network
 * @param inputs Input sequence [seq_len, n_inputs]
 * @param outputs Output sequence [seq_len, n_outputs]
 * @param seq_len Sequence length
 * @param dt Time step per sample
 * @return 0 on success
 */
int lnn_forward_sequence(
    lnn_network_t* network,
    const nimcp_tensor_t* inputs,
    nimcp_tensor_t* outputs,
    uint32_t seq_len,
    float dt
);

/**
 * @brief Parallel forward over batch of sequences
 */
int lnn_forward_batch(
    lnn_network_t* network,
    const nimcp_tensor_t* inputs,   /**< [batch, seq_len, n_inputs] */
    nimcp_tensor_t* outputs,        /**< [batch, seq_len, n_outputs] */
    uint32_t batch_size,
    uint32_t seq_len,
    float dt
);

/*=============================================================================
 * Training
 *===========================================================================*/

/**
 * @brief Set training mode
 */
void lnn_set_training(lnn_network_t* network, bool training);

/**
 * @brief Backward pass using adjoint method
 *
 * WHAT: Compute gradients for all parameters
 * WHY:  Enable learning with continuous-time dynamics
 * HOW:  Solve adjoint ODE backwards in time
 *
 * @param network LNN network
 * @param loss_grad Gradient of loss w.r.t. outputs [seq_len, n_outputs]
 * @return 0 on success
 */
int lnn_backward(
    lnn_network_t* network,
    const nimcp_tensor_t* loss_grad
);

/**
 * @brief Reset network state (between sequences)
 */
void lnn_reset_state(lnn_network_t* network);

/**
 * @brief Reset gradients (between batches)
 */
void lnn_reset_gradients(lnn_network_t* network);

/*=============================================================================
 * State Access
 *===========================================================================*/

/**
 * @brief Get current network state
 */
int lnn_get_state(const lnn_network_t* network, nimcp_tensor_t* state);

/**
 * @brief Set network state
 */
int lnn_set_state(lnn_network_t* network, const nimcp_tensor_t* state);

/**
 * @brief Get current time constants
 */
int lnn_get_tau(const lnn_network_t* network, nimcp_tensor_t* tau);

/**
 * @brief Get network statistics
 */
int lnn_get_stats(const lnn_network_t* network, lnn_network_stats_t* stats);

/*=============================================================================
 * Integration
 *===========================================================================*/

/**
 * @brief Connect to NIMCP optimizer
 */
int lnn_connect_optimizer(
    lnn_network_t* network,
    void* optimizer  /* nimcp_optimizer_context_t* */
);

/**
 * @brief Connect to bio-async router
 */
int lnn_connect_bio_async(lnn_network_t* network);

/**
 * @brief Connect to immune system
 */
int lnn_connect_immune(
    lnn_network_t* network,
    void* immune_bridge  /* lnn_immune_bridge_t* */
);

/*=============================================================================
 * Serialization
 *===========================================================================*/

/**
 * @brief Save network to file
 */
int lnn_save(const lnn_network_t* network, const char* path);

/**
 * @brief Load network from file
 */
lnn_network_t* lnn_load(const char* path);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_H */
```

---

## 3. Integration Infrastructure

### 3.1 Module Adapter Pattern

Each NIMCP module gets an LNN adapter that maps module state to/from LNN state.

```c
/**
 * @file nimcp_lnn_adapter.h
 * @brief Base adapter interface for LNN-module integration
 */

#ifndef NIMCP_LNN_ADAPTER_H
#define NIMCP_LNN_ADAPTER_H

#include "lnn/nimcp_lnn.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Function pointer types for state mapping
 */
typedef int (*lnn_state_to_module_fn)(
    const nimcp_tensor_t* lnn_state,
    void* module_effects,
    void* user_data
);

typedef int (*module_to_lnn_state_fn)(
    const void* module_state,
    nimcp_tensor_t* lnn_input,
    void* user_data
);

/**
 * @brief Generic LNN adapter for any NIMCP module
 */
typedef struct {
    /* LNN network */
    lnn_network_t* network;

    /* Mapping functions */
    lnn_state_to_module_fn state_to_effects;
    module_to_lnn_state_fn state_to_input;
    void* user_data;

    /* Module handle (opaque) */
    void* module;

    /* Configuration */
    float update_interval_ms;
    float last_update_time;

    /* Bio-async */
    uint16_t bio_module_id;
    bool bio_async_enabled;

    /* Thread safety */
    void* mutex;

    /* Statistics */
    uint64_t updates;
    double total_compute_time_ms;
} lnn_adapter_t;

/**
 * @brief Create adapter for module
 */
lnn_adapter_t* lnn_adapter_create(
    const lnn_config_t* lnn_config,
    void* module,
    lnn_state_to_module_fn state_to_effects,
    module_to_lnn_state_fn state_to_input,
    void* user_data
);

/**
 * @brief Update adapter (forward pass + effect application)
 */
int lnn_adapter_update(lnn_adapter_t* adapter, float dt);

/**
 * @brief Destroy adapter
 */
void lnn_adapter_destroy(lnn_adapter_t* adapter);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_ADAPTER_H */
```

### 3.2 Bio-async Module IDs

```c
/* Add to include/async/nimcp_bio_messages.h */

/* LNN modules (0x0E00 - 0x0EFF) */
#define BIO_MODULE_LNN_CORE                 0x0E00
#define BIO_MODULE_LNN_CALCIUM              0x0E01
#define BIO_MODULE_LNN_STP                  0x0E02
#define BIO_MODULE_LNN_ELIGIBILITY          0x0E03
#define BIO_MODULE_LNN_OSCILLATIONS         0x0E04
#define BIO_MODULE_LNN_CORTICAL_TEMPORAL    0x0E05
#define BIO_MODULE_LNN_PREDICTIVE           0x0E06
#define BIO_MODULE_LNN_SEQUENCE             0x0E07
#define BIO_MODULE_LNN_WORKING_MEMORY       0x0E08
#define BIO_MODULE_LNN_SLEEP                0x0E09
#define BIO_MODULE_LNN_AUDIO                0x0E0A
#define BIO_MODULE_LNN_SPEECH               0x0E0B
#define BIO_MODULE_LNN_VISUAL               0x0E0C
#define BIO_MODULE_LNN_HOMEOSTATIC          0x0E0D
#define BIO_MODULE_LNN_NEUROMOD             0x0E0E
#define BIO_MODULE_LNN_EMOTION              0x0E0F
#define BIO_MODULE_LNN_ATTENTION            0x0E10
#define BIO_MODULE_LNN_POPULATION           0x0E11
#define BIO_MODULE_LNN_TEMPORAL_CODING      0x0E12
#define BIO_MODULE_LNN_SECOND_MESSENGER     0x0E13
#define BIO_MODULE_LNN_MOTOR                0x0E14
#define BIO_MODULE_LNN_LANGUAGE             0x0E15
#define BIO_MODULE_LNN_TIMESCALES           0x0E16
#define BIO_MODULE_LNN_SYNCHRONY            0x0E17
#define BIO_MODULE_LNN_DENDRITIC            0x0E18

/* LNN message types (0x0E00 - 0x0EFF) */
#define BIO_MSG_LNN_STATE_UPDATE            0x0E00
#define BIO_MSG_LNN_TAU_CHANGED             0x0E01
#define BIO_MSG_LNN_GRADIENT_READY          0x0E02
#define BIO_MSG_LNN_INSTABILITY_DETECTED    0x0E03
#define BIO_MSG_LNN_STEP_COMPLETE           0x0E04
#define BIO_MSG_LNN_TRAINING_EVENT          0x0E05
```

---

## 4. Module Integration Phases

### Phase 1: Foundation (Weeks 1-2)
**Core LNN library and calcium dynamics**

| Module | Files | Tests | Priority |
|--------|-------|-------|----------|
| LNN Core Library | 12 | 150 | Critical |
| Calcium Dynamics | 4 | 30 | Critical |
| STP (Short-Term Plasticity) | 4 | 30 | Critical |

**Deliverables:**
- Complete LNN library with all solvers
- Calcium dynamics LNN adapter
- STP LNN adapter
- Full test coverage

### Phase 2: Plasticity (Weeks 3-4)
**Eligibility traces and homeostatic mechanisms**

| Module | Files | Tests | Priority |
|--------|-------|-------|----------|
| Eligibility Traces | 4 | 25 | High |
| Homeostatic Plasticity | 4 | 25 | High |
| Second Messengers | 4 | 25 | High |
| Neuromodulators | 4 | 25 | High |

### Phase 3: Oscillations (Weeks 5-6)
**Brain oscillations and temporal integration**

| Module | Files | Tests | Priority |
|--------|-------|-------|----------|
| Brain Oscillations | 4 | 30 | High |
| Cortical Temporal | 4 | 25 | High |
| Synchrony Detector | 4 | 20 | Medium |
| Oscillation Detector | 4 | 20 | Medium |

### Phase 4: Prediction (Weeks 7-8)
**Predictive coding and temporal patterns**

| Module | Files | Tests | Priority |
|--------|-------|-------|----------|
| Predictive Coding | 4 | 30 | High |
| Temporal Patterns | 4 | 25 | High |
| Sequence Detector | 4 | 25 | Medium |

### Phase 5: Memory (Weeks 9-10)
**Working memory and sleep systems**

| Module | Files | Tests | Priority |
|--------|-------|-------|----------|
| Working Memory | 4 | 30 | High |
| Sleep-Wake Cycle | 4 | 30 | High |
| Circular Buffer | 4 | 20 | Medium |

### Phase 6: Sensory (Weeks 11-12)
**Audio, speech, and visual processing**

| Module | Files | Tests | Priority |
|--------|-------|-------|----------|
| Audio Cortex | 4 | 30 | High |
| Speech Cortex | 4 | 30 | High |
| Visual Cortex | 4 | 30 | High |

### Phase 7: Cognition (Weeks 13-14)
**Attention and emotion systems**

| Module | Files | Tests | Priority |
|--------|-------|-------|----------|
| Emotion-Attention | 4 | 25 | Medium |
| Emotional System | 4 | 25 | Medium |
| Thalamic Router | 4 | 20 | Medium |

### Phase 8: Encoding (Weeks 15-16)
**Population coding and temporal encoding**

| Module | Files | Tests | Priority |
|--------|-------|-------|----------|
| Population Coding | 4 | 25 | Medium |
| Temporal Coding | 4 | 20 | Medium |
| Rate Coding | 4 | 20 | Medium |
| Motor/Language | 4 | 25 | Medium |

---

## 5. Training Layer Integration

### 5.1 LNN Optimizer Integration

```c
/**
 * @file nimcp_lnn_training.h
 * @brief Training integration for LNN networks
 */

#ifndef NIMCP_LNN_TRAINING_H
#define NIMCP_LNN_TRAINING_H

#include "nimcp_lnn_types.h"
#include "middleware/training/nimcp_optimizers.h"
#include "middleware/training/nimcp_gradient_manager.h"
#include "middleware/training/nimcp_loss_functions.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LNN training context
 */
typedef struct {
    /* LNN network */
    lnn_network_t* network;

    /* NIMCP training components */
    nimcp_optimizer_context_t* optimizer;
    nimcp_gradient_manager_ctx_t* gradient_manager;
    nimcp_loss_context_t* loss_context;

    /* Training state */
    uint64_t step_count;
    uint64_t epoch_count;
    float current_lr;
    float current_loss;

    /* Gradient handling */
    bool accumulate_gradients;
    uint32_t accumulation_steps;
    uint32_t current_accumulation;

    /* Training bridges */
    void* cognitive_training_bridge;   /**< cognitive_training_bridge_t* */
    void* training_logic_bridge;       /**< training_logic_bridge_t* */
    void* training_immune_bridge;      /**< training_immune_system_t* */
    void* training_plasticity_bridge;  /**< tpb_context_t* */

    /* Bio-async */
    bool bio_async_enabled;
    void* bio_ctx;

    /* Callbacks */
    void (*on_step_complete)(void* user_data, uint64_t step, float loss);
    void (*on_epoch_complete)(void* user_data, uint64_t epoch, float avg_loss);
    void (*on_lr_change)(void* user_data, float old_lr, float new_lr);
    void* callback_user_data;

    /* Thread safety */
    void* mutex;
} lnn_training_ctx_t;

/**
 * @brief Configuration for LNN training
 */
typedef struct {
    /* Optimizer */
    nimcp_optimizer_type_t optimizer_type;
    float learning_rate;
    float weight_decay;

    /* Adam-specific */
    float beta1;
    float beta2;
    float epsilon;

    /* Gradient */
    float gradient_clip_norm;
    bool use_gradient_scaling;
    nimcp_grad_accum_mode_t accum_mode;
    uint32_t accumulation_steps;

    /* Loss */
    nimcp_loss_type_t loss_type;
    nimcp_loss_reduction_t reduction;

    /* LNN-specific */
    lnn_train_mode_t lnn_train_mode;
    uint32_t bptt_truncation;
    bool use_adjoint_checkpointing;

    /* Integrations */
    bool enable_cognitive_integration;
    bool enable_logic_integration;
    bool enable_immune_integration;
    bool enable_plasticity_integration;
    bool enable_bio_async;
} lnn_training_config_t;

/*=============================================================================
 * Training API
 *===========================================================================*/

/**
 * @brief Create training context for LNN network
 */
lnn_training_ctx_t* lnn_training_create(
    lnn_network_t* network,
    const lnn_training_config_t* config
);

/**
 * @brief Destroy training context
 */
void lnn_training_destroy(lnn_training_ctx_t* ctx);

/**
 * @brief Execute one training step
 *
 * WHAT: Forward + backward + optimizer step
 * WHY:  Core training loop operation
 * HOW:  1. Forward pass 2. Compute loss 3. Backward (adjoint) 4. Optimizer step
 *
 * @param ctx Training context
 * @param inputs Input sequence [seq_len, n_inputs]
 * @param targets Target sequence [seq_len, n_outputs]
 * @param loss_out Output: computed loss value
 * @return 0 on success
 */
int lnn_training_step(
    lnn_training_ctx_t* ctx,
    const nimcp_tensor_t* inputs,
    const nimcp_tensor_t* targets,
    float* loss_out
);

/**
 * @brief Execute training step on batch (parallel)
 */
int lnn_training_step_batch(
    lnn_training_ctx_t* ctx,
    const nimcp_tensor_t* inputs,    /**< [batch, seq_len, n_inputs] */
    const nimcp_tensor_t* targets,   /**< [batch, seq_len, n_outputs] */
    uint32_t batch_size,
    float* loss_out
);

/**
 * @brief Train for one epoch
 */
int lnn_training_epoch(
    lnn_training_ctx_t* ctx,
    const nimcp_tensor_t* dataset_inputs,
    const nimcp_tensor_t* dataset_targets,
    uint32_t n_samples,
    uint32_t batch_size,
    float* avg_loss_out
);

/*=============================================================================
 * Integration Connections
 *===========================================================================*/

/**
 * @brief Connect to cognitive training bridge
 */
int lnn_training_connect_cognitive(
    lnn_training_ctx_t* ctx,
    void* cognitive_bridge
);

/**
 * @brief Connect to training logic bridge
 */
int lnn_training_connect_logic(
    lnn_training_ctx_t* ctx,
    void* logic_bridge
);

/**
 * @brief Connect to training immune system
 */
int lnn_training_connect_immune(
    lnn_training_ctx_t* ctx,
    void* immune_system
);

/**
 * @brief Connect to training plasticity bridge
 */
int lnn_training_connect_plasticity(
    lnn_training_ctx_t* ctx,
    void* plasticity_bridge
);

/*=============================================================================
 * Learning Rate Scheduling
 *===========================================================================*/

typedef enum {
    LNN_LR_SCHEDULE_CONSTANT = 0,
    LNN_LR_SCHEDULE_STEP,
    LNN_LR_SCHEDULE_EXPONENTIAL,
    LNN_LR_SCHEDULE_COSINE,
    LNN_LR_SCHEDULE_WARMUP_COSINE,
    LNN_LR_SCHEDULE_REDUCE_ON_PLATEAU
} lnn_lr_schedule_t;

/**
 * @brief Set learning rate schedule
 */
int lnn_training_set_lr_schedule(
    lnn_training_ctx_t* ctx,
    lnn_lr_schedule_t schedule,
    float* params,
    uint32_t n_params
);

/**
 * @brief Update learning rate (call after each step/epoch)
 */
int lnn_training_update_lr(lnn_training_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_TRAINING_H */
```

### 5.2 Adjoint Gradient Computation

```c
/**
 * @file nimcp_lnn_gradient.h
 * @brief Adjoint method for gradient computation in LNN
 *
 * WHAT: Memory-efficient gradient computation for continuous-time networks
 * WHY:  BPTT requires O(T) memory, adjoint requires O(1)
 * HOW:  Solve adjoint ODE backwards: dλ/dt = -∂f/∂x^T λ - ∂L/∂x
 */

#ifndef NIMCP_LNN_GRADIENT_H
#define NIMCP_LNN_GRADIENT_H

#include "nimcp_lnn_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create gradient context for adjoint method
 */
lnn_gradient_ctx_t* lnn_gradient_create(
    lnn_network_t* network,
    uint32_t max_steps,
    bool use_checkpointing,
    uint32_t checkpoint_interval
);

/**
 * @brief Destroy gradient context
 */
void lnn_gradient_destroy(lnn_gradient_ctx_t* ctx);

/**
 * @brief Compute gradients using adjoint method
 *
 * @param ctx Gradient context
 * @param network LNN network (with recorded forward pass)
 * @param dL_dx_final Gradient of loss w.r.t. final state
 * @return 0 on success
 */
int lnn_gradient_compute_adjoint(
    lnn_gradient_ctx_t* ctx,
    lnn_network_t* network,
    const nimcp_tensor_t* dL_dx_final
);

/**
 * @brief Compute gradients using BPTT
 */
int lnn_gradient_compute_bptt(
    lnn_gradient_ctx_t* ctx,
    lnn_network_t* network,
    const nimcp_tensor_t* dL_dx_sequence
);

/**
 * @brief Get accumulated parameter gradients
 */
int lnn_gradient_get_params(
    const lnn_gradient_ctx_t* ctx,
    nimcp_tensor_t* grad_params
);

/**
 * @brief Apply gradients to network using optimizer
 */
int lnn_gradient_apply(
    lnn_gradient_ctx_t* ctx,
    lnn_network_t* network,
    nimcp_optimizer_context_t* optimizer
);

/**
 * @brief Parallel gradient computation across batch
 */
int lnn_gradient_compute_batch_parallel(
    lnn_gradient_ctx_t* ctx,
    lnn_network_t* network,
    const nimcp_tensor_t* dL_dx_batch,  /**< [batch, seq_len, n_outputs] */
    uint32_t batch_size,
    void* thread_pool
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_GRADIENT_H */
```

---

## 6. Bio-Async Integration

### 6.1 LNN Bio-Async Bridge

```c
/**
 * @file nimcp_lnn_bio_async.h
 * @brief Bio-async integration for LNN networks
 */

#ifndef NIMCP_LNN_BIO_ASYNC_H
#define NIMCP_LNN_BIO_ASYNC_H

#include "nimcp_lnn_types.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bio-async message types for LNN
 */
typedef enum {
    LNN_BIO_MSG_STATE_BROADCAST = 0,    /**< Broadcast current state */
    LNN_BIO_MSG_TAU_UPDATE,             /**< Time constant changed */
    LNN_BIO_MSG_GRADIENT_READY,         /**< Gradients computed */
    LNN_BIO_MSG_INSTABILITY,            /**< Detected instability */
    LNN_BIO_MSG_SYNC_REQUEST,           /**< Request phase sync */
    LNN_BIO_MSG_SYNC_RESPONSE,          /**< Response to sync request */
    LNN_BIO_MSG_TRAINING_EVENT,         /**< Training step/epoch event */
    LNN_BIO_MSG_COUNT
} lnn_bio_msg_type_t;

/**
 * @brief State broadcast message
 */
typedef struct {
    uint32_t network_id;
    uint32_t layer_id;
    float* state;               /**< Current state values */
    uint32_t state_size;
    float* tau;                 /**< Current time constants */
    uint32_t tau_size;
    float t;                    /**< Current time */
    lnn_state_health_t health;
} lnn_bio_state_msg_t;

/**
 * @brief Phase sync message
 */
typedef struct {
    uint32_t network_id;
    nimcp_oscillation_band_t band;
    float phase;                /**< Current phase [0, 2π] */
    float frequency;            /**< Current frequency (Hz) */
    float coherence_target;     /**< Desired coherence level */
} lnn_bio_sync_msg_t;

/**
 * @brief Training event message
 */
typedef struct {
    uint32_t network_id;
    uint64_t step;
    uint64_t epoch;
    float loss;
    float lr;
    float gradient_norm;
    bool is_step_complete;
    bool is_epoch_complete;
} lnn_bio_training_msg_t;

/*=============================================================================
 * Bio-Async API
 *===========================================================================*/

/**
 * @brief Connect LNN network to bio-async router
 */
int lnn_bio_async_connect(
    lnn_network_t* network,
    uint16_t module_id
);

/**
 * @brief Disconnect from bio-async router
 */
int lnn_bio_async_disconnect(lnn_network_t* network);

/**
 * @brief Check if connected
 */
bool lnn_bio_async_is_connected(const lnn_network_t* network);

/**
 * @brief Broadcast current state to other modules
 */
int lnn_bio_async_broadcast_state(
    lnn_network_t* network,
    uint16_t target_module  /**< BIO_MODULE_ALL for broadcast */
);

/**
 * @brief Request phase synchronization
 */
int lnn_bio_async_request_sync(
    lnn_network_t* network,
    nimcp_oscillation_band_t band,
    float coherence_target
);

/**
 * @brief Register handler for incoming messages
 */
typedef int (*lnn_bio_msg_handler_t)(
    lnn_network_t* network,
    lnn_bio_msg_type_t type,
    const void* msg,
    size_t msg_size,
    void* user_data
);

int lnn_bio_async_register_handler(
    lnn_network_t* network,
    lnn_bio_msg_type_t type,
    lnn_bio_msg_handler_t handler,
    void* user_data
);

/**
 * @brief Process pending bio-async messages
 */
int lnn_bio_async_process(lnn_network_t* network, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_BIO_ASYNC_H */
```

---

## 7. Immune System Integration

### 7.1 LNN Immune Bridge

```c
/**
 * @file nimcp_lnn_immune.h
 * @brief Immune system integration for LNN networks
 *
 * WHAT: Bidirectional immune-LNN integration
 * WHY:  LNN instabilities are threats; immune modulates LNN dynamics
 * HOW:  Report instabilities as antigens; apply cytokine effects to τ
 */

#ifndef NIMCP_LNN_IMMUNE_H
#define NIMCP_LNN_IMMUNE_H

#include "nimcp_lnn_types.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "middleware/immune/nimcp_training_immune.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LNN instability types (map to immune antigens)
 */
typedef enum {
    LNN_INSTABILITY_NONE = 0,
    LNN_INSTABILITY_NAN_STATE,          /**< NaN in state vector */
    LNN_INSTABILITY_INF_STATE,          /**< Inf in state vector */
    LNN_INSTABILITY_STATE_EXPLOSION,    /**< ||x|| > threshold */
    LNN_INSTABILITY_STATE_COLLAPSE,     /**< ||x|| < threshold */
    LNN_INSTABILITY_TAU_EXPLOSION,      /**< τ > max_tau */
    LNN_INSTABILITY_TAU_COLLAPSE,       /**< τ < min_tau */
    LNN_INSTABILITY_GRADIENT_EXPLOSION, /**< ||∇|| > threshold */
    LNN_INSTABILITY_GRADIENT_VANISHING, /**< ||∇|| < threshold */
    LNN_INSTABILITY_ODE_DIVERGENCE,     /**< ODE solver diverged */
    LNN_INSTABILITY_COUNT
} lnn_instability_type_t;

/**
 * @brief Cytokine effects on LNN dynamics
 */
typedef struct {
    /* τ modulation (fever model) */
    float tau_scale;            /**< Scale factor for time constants */
    float tau_offset;           /**< Additive offset to time constants */

    /* Learning modulation */
    float lr_factor;            /**< Learning rate multiplier */
    float gradient_scale;       /**< Gradient scaling factor */

    /* State modulation */
    float state_damping;        /**< Damping factor for state updates */
    float noise_injection;      /**< Noise level for exploration */

    /* Inflammation level */
    brain_inflammation_level_t inflammation;

    /* Validity */
    bool valid;
} lnn_cytokine_effects_t;

/**
 * @brief LNN immune bridge configuration
 */
typedef struct {
    /* Instability thresholds */
    float state_explosion_threshold;    /**< Default: 1e6 */
    float state_collapse_threshold;     /**< Default: 1e-10 */
    float tau_max;                      /**< Default: 1000 ms */
    float tau_min;                      /**< Default: 0.1 ms */
    float gradient_explosion_threshold; /**< Default: 1e3 */
    float gradient_vanishing_threshold; /**< Default: 1e-7 */

    /* Immune response */
    bool auto_report_instabilities;
    uint8_t instability_severity[LNN_INSTABILITY_COUNT];

    /* Cytokine modulation */
    bool enable_tau_modulation;
    bool enable_lr_modulation;
    bool enable_state_damping;
    float tau_inflammation_scales[5];   /**< Per inflammation level */
    float lr_inflammation_factors[5];

    /* Bio-async */
    bool enable_bio_async;
} lnn_immune_config_t;

/**
 * @brief LNN immune bridge
 */
typedef struct {
    /* LNN network */
    lnn_network_t* network;

    /* Immune systems */
    brain_immune_system_t* brain_immune;
    training_immune_system_t* training_immune;

    /* Configuration */
    lnn_immune_config_t config;

    /* Current effects */
    lnn_cytokine_effects_t cytokine_effects;

    /* Statistics */
    uint64_t instabilities_detected;
    uint64_t instabilities_reported;
    uint64_t immune_updates;

    /* Bio-async */
    bool bio_async_enabled;
    void* bio_ctx;

    /* Thread safety */
    void* mutex;
} lnn_immune_bridge_t;

/*=============================================================================
 * Immune Bridge API
 *===========================================================================*/

/**
 * @brief Default configuration
 */
int lnn_immune_config_default(lnn_immune_config_t* config);

/**
 * @brief Create immune bridge for LNN network
 */
lnn_immune_bridge_t* lnn_immune_bridge_create(
    lnn_network_t* network,
    const lnn_immune_config_t* config
);

/**
 * @brief Destroy immune bridge
 */
void lnn_immune_bridge_destroy(lnn_immune_bridge_t* bridge);

/**
 * @brief Connect to brain immune system
 */
int lnn_immune_connect_brain_immune(
    lnn_immune_bridge_t* bridge,
    brain_immune_system_t* brain_immune
);

/**
 * @brief Connect to training immune system
 */
int lnn_immune_connect_training_immune(
    lnn_immune_bridge_t* bridge,
    training_immune_system_t* training_immune
);

/**
 * @brief Check LNN state for instabilities
 *
 * WHAT: Scan network state for instabilities
 * WHY:  Early detection enables immune response
 * HOW:  Check state norms, τ bounds, gradient health
 *
 * @return Detected instability type (LNN_INSTABILITY_NONE if healthy)
 */
lnn_instability_type_t lnn_immune_check_stability(
    lnn_immune_bridge_t* bridge
);

/**
 * @brief Report instability to immune system
 */
int lnn_immune_report_instability(
    lnn_immune_bridge_t* bridge,
    lnn_instability_type_t type,
    uint32_t layer_id,
    uint32_t neuron_id
);

/**
 * @brief Update cytokine effects from immune system
 */
int lnn_immune_update_effects(lnn_immune_bridge_t* bridge);

/**
 * @brief Apply cytokine effects to LNN dynamics
 */
int lnn_immune_apply_effects(lnn_immune_bridge_t* bridge);

/**
 * @brief Get current cytokine effects
 */
int lnn_immune_get_effects(
    const lnn_immune_bridge_t* bridge,
    lnn_cytokine_effects_t* effects
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_IMMUNE_H */
```

---

## 8. Parallelization Strategy

### 8.1 Parallel Computation Levels

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      LNN PARALLELIZATION HIERARCHY                          │
└─────────────────────────────────────────────────────────────────────────────┘

Level 1: BATCH PARALLELISM
┌─────────────────────────────────────────────────────────────────────────────┐
│  Thread 0    │  Thread 1    │  Thread 2    │  Thread 3    │  ...           │
│  ┌─────────┐ │  ┌─────────┐ │  ┌─────────┐ │  ┌─────────┐ │                │
│  │ Batch 0 │ │  │ Batch 1 │ │  │ Batch 2 │ │  │ Batch 3 │ │                │
│  │ Seq 0-N │ │  │ Seq 0-N │ │  │ Seq 0-N │ │  │ Seq 0-N │ │                │
│  └─────────┘ │  └─────────┘ │  └─────────┘ │  └─────────┘ │                │
└─────────────────────────────────────────────────────────────────────────────┘

Level 2: LAYER PARALLELISM (Pipeline)
┌─────────────────────────────────────────────────────────────────────────────┐
│  Time T     │  Time T+1   │  Time T+2   │  Time T+3   │  Time T+4         │
│  Layer 0    │  Layer 0    │  Layer 0    │  Layer 0    │  Layer 0          │
│  Input[T]   │  Input[T+1] │  Input[T+2] │  Input[T+3] │  Input[T+4]       │
│      ↓      │      ↓      │      ↓      │      ↓      │      ↓            │
│  Layer 1    │  Layer 1    │  Layer 1    │  Layer 1    │  Layer 1          │
│  State[T-1] │  State[T]   │  State[T+1] │  State[T+2] │  State[T+3]       │
│      ↓      │      ↓      │      ↓      │      ↓      │      ↓            │
│  Layer 2    │  Layer 2    │  Layer 2    │  Layer 2    │  Layer 2          │
│  State[T-2] │  State[T-1] │  State[T]   │  State[T+1] │  State[T+2]       │
└─────────────────────────────────────────────────────────────────────────────┘

Level 3: NEURON PARALLELISM (SIMD)
┌─────────────────────────────────────────────────────────────────────────────┐
│  AVX-512: 16 neurons per vector operation                                   │
│  ┌───────┬───────┬───────┬───────┬───────┬───────┬───────┬───────┐         │
│  │ N[0]  │ N[1]  │ N[2]  │ N[3]  │ N[4]  │ N[5]  │ N[6]  │ N[7]  │ ...     │
│  │ x[0]  │ x[1]  │ x[2]  │ x[3]  │ x[4]  │ x[5]  │ x[6]  │ x[7]  │         │
│  │ τ[0]  │ τ[1]  │ τ[2]  │ τ[3]  │ τ[4]  │ τ[5]  │ τ[6]  │ τ[7]  │         │
│  └───────┴───────┴───────┴───────┴───────┴───────┴───────┴───────┘         │
│                            SIMD OPERATIONS                                  │
│  x_new = x + dt * (-x/τ + f(W*input + W_rec*x + b))                        │
└─────────────────────────────────────────────────────────────────────────────┘

Level 4: ODE SOLVER PARALLELISM
┌─────────────────────────────────────────────────────────────────────────────┐
│  RK4: 4 parallel function evaluations per step                             │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐                        │
│  │   k1    │  │   k2    │  │   k3    │  │   k4    │                        │
│  │ f(t,x)  │  │ f(t+h/2,│  │ f(t+h/2,│  │ f(t+h,  │                        │
│  │         │  │  x+k1/2)│  │  x+k2/2)│  │  x+k3)  │                        │
│  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘                        │
│       └────────────┴────────────┴────────────┘                             │
│                        REDUCTION                                            │
│       x_new = x + (k1 + 2*k2 + 2*k3 + k4) / 6                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 8.2 Parallel API

```c
/**
 * @file nimcp_lnn_parallel.h
 * @brief Parallelization primitives for LNN computation
 */

#ifndef NIMCP_LNN_PARALLEL_H
#define NIMCP_LNN_PARALLEL_H

#include "nimcp_lnn_types.h"
#include "utils/thread/nimcp_thread_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parallel execution configuration
 */
typedef struct {
    uint32_t n_threads;
    bool enable_batch_parallel;
    bool enable_layer_pipeline;
    bool enable_simd;
    bool enable_ode_parallel;
    size_t batch_chunk_size;
    size_t neuron_simd_width;   /**< Auto-detect if 0 */
} lnn_parallel_config_t;

/**
 * @brief Batch parallel context
 */
typedef struct lnn_batch_parallel_ctx_s lnn_batch_parallel_ctx_t;

/**
 * @brief Create batch parallel context
 */
lnn_batch_parallel_ctx_t* lnn_batch_parallel_create(
    lnn_network_t* network,
    const lnn_parallel_config_t* config
);

/**
 * @brief Destroy batch parallel context
 */
void lnn_batch_parallel_destroy(lnn_batch_parallel_ctx_t* ctx);

/**
 * @brief Execute forward pass in parallel across batch
 */
int lnn_batch_parallel_forward(
    lnn_batch_parallel_ctx_t* ctx,
    const nimcp_tensor_t* inputs,   /**< [batch, seq_len, n_inputs] */
    nimcp_tensor_t* outputs,        /**< [batch, seq_len, n_outputs] */
    uint32_t batch_size,
    uint32_t seq_len,
    float dt
);

/**
 * @brief Execute backward pass in parallel across batch
 */
int lnn_batch_parallel_backward(
    lnn_batch_parallel_ctx_t* ctx,
    const nimcp_tensor_t* loss_grads, /**< [batch, seq_len, n_outputs] */
    uint32_t batch_size
);

/**
 * @brief SIMD-optimized layer forward
 */
int lnn_layer_forward_simd(
    lnn_layer_t* layer,
    const nimcp_tensor_t* input,
    nimcp_tensor_t* output,
    float dt
);

/**
 * @brief SIMD-optimized ODE step
 */
int lnn_ode_step_simd(
    nimcp_tensor_t* x,
    const nimcp_tensor_t* dx_dt,
    nimcp_tensor_t* tau,
    float dt,
    lnn_ode_method_t method
);

/**
 * @brief Pipeline layer execution
 */
typedef struct lnn_pipeline_ctx_s lnn_pipeline_ctx_t;

lnn_pipeline_ctx_t* lnn_pipeline_create(
    lnn_network_t* network,
    uint32_t pipeline_depth
);

int lnn_pipeline_submit(
    lnn_pipeline_ctx_t* ctx,
    const nimcp_tensor_t* input,
    float dt
);

int lnn_pipeline_get_output(
    lnn_pipeline_ctx_t* ctx,
    nimcp_tensor_t* output,
    int timeout_ms
);

void lnn_pipeline_destroy(lnn_pipeline_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_PARALLEL_H */
```

---

## 9. Testing Strategy

### 9.1 Test Categories

| Category | Description | Count |
|----------|-------------|-------|
| Unit Tests | Individual component tests | 600 |
| Integration Tests | Cross-component tests | 300 |
| Regression Tests | Stability and accuracy | 200 |
| E2E Tests | Full pipeline tests | 100 |
| **Total** | | **1,200** |

### 9.2 Test File Structure

```
test/
├── unit/lnn/
│   ├── test_lnn_neuron.cpp           # 50 tests
│   ├── test_lnn_layer.cpp            # 50 tests
│   ├── test_lnn_network.cpp          # 50 tests
│   ├── test_lnn_ode.cpp              # 50 tests
│   ├── test_lnn_wiring.cpp           # 40 tests
│   ├── test_lnn_gradient.cpp         # 50 tests
│   ├── test_lnn_training.cpp         # 50 tests
│   ├── test_lnn_parallel.cpp         # 40 tests
│   ├── test_lnn_bio_async.cpp        # 30 tests
│   └── test_lnn_immune.cpp           # 30 tests
│
├── unit/lnn/adapters/
│   ├── test_lnn_calcium_adapter.cpp  # 30 tests
│   ├── test_lnn_stp_adapter.cpp      # 30 tests
│   ├── test_lnn_eligibility_adapter.cpp # 25 tests
│   ├── ... (20+ adapter tests)
│
├── integration/lnn/
│   ├── test_lnn_optimizer_integration.cpp     # 30 tests
│   ├── test_lnn_gradient_manager_integration.cpp # 25 tests
│   ├── test_lnn_loss_integration.cpp          # 25 tests
│   ├── test_lnn_cognitive_training_integration.cpp # 20 tests
│   ├── test_lnn_training_logic_integration.cpp # 20 tests
│   ├── test_lnn_immune_integration.cpp        # 25 tests
│   ├── test_lnn_bio_async_integration.cpp     # 25 tests
│   ├── test_lnn_tensor_integration.cpp        # 25 tests
│   ├── test_lnn_parallel_integration.cpp      # 25 tests
│   └── test_lnn_module_adapters_integration.cpp # 30 tests
│
├── regression/lnn/
│   ├── test_lnn_numerical_stability.cpp       # 40 tests
│   ├── test_lnn_gradient_accuracy.cpp         # 30 tests
│   ├── test_lnn_ode_accuracy.cpp              # 30 tests
│   ├── test_lnn_memory_usage.cpp              # 25 tests
│   ├── test_lnn_performance.cpp               # 25 tests
│   └── test_lnn_thread_safety.cpp             # 25 tests
│
└── e2e/
    ├── e2e_test_lnn_calcium_pipeline.cpp      # 10 tests
    ├── e2e_test_lnn_oscillation_pipeline.cpp  # 10 tests
    ├── e2e_test_lnn_prediction_pipeline.cpp   # 10 tests
    ├── e2e_test_lnn_sensory_pipeline.cpp      # 10 tests
    ├── e2e_test_lnn_full_training_pipeline.cpp # 15 tests
    └── e2e_test_lnn_unified_integration.cpp   # 20 tests
```

### 9.3 Key Test Scenarios

```c
/**
 * Test: ODE solver accuracy
 * Verify RK4 matches analytical solution for simple ODE
 */
TEST(LnnOde, RK4AccuracyExponentialDecay) {
    // dx/dt = -x/τ has solution x(t) = x0 * exp(-t/τ)
    float x0 = 1.0f;
    float tau = 10.0f;
    float dt = 0.1f;
    float t_final = 100.0f;

    float x_numerical = x0;
    for (float t = 0; t < t_final; t += dt) {
        x_numerical = lnn_ode_step_rk4(x_numerical, tau, dt);
    }

    float x_analytical = x0 * expf(-t_final / tau);
    EXPECT_NEAR(x_numerical, x_analytical, 1e-4);
}

/**
 * Test: Gradient computation correctness
 * Verify adjoint gradients match finite differences
 */
TEST(LnnGradient, AdjointMatchesFiniteDiff) {
    lnn_network_t* net = lnn_network_create_ncp(4, 8, 4, 2);
    nimcp_tensor_t* input = nimcp_tensor_randn((uint32_t[]){10, 4}, 2);
    nimcp_tensor_t* target = nimcp_tensor_randn((uint32_t[]){10, 2}, 2);

    // Compute adjoint gradients
    lnn_forward_sequence(net, input, NULL, 10, 1.0f);
    nimcp_tensor_t* loss_grad = compute_mse_grad(net->output, target);
    lnn_backward(net, loss_grad);

    // Compare to finite differences
    float eps = 1e-4f;
    for (size_t i = 0; i < net->n_params; i++) {
        float orig = get_param(net, i);

        set_param(net, i, orig + eps);
        float loss_plus = compute_loss(net, input, target);

        set_param(net, i, orig - eps);
        float loss_minus = compute_loss(net, input, target);

        set_param(net, i, orig);

        float fd_grad = (loss_plus - loss_minus) / (2 * eps);
        float adj_grad = get_grad(net, i);

        EXPECT_NEAR(fd_grad, adj_grad, 1e-3);
    }
}

/**
 * Test: Immune response to instability
 */
TEST(LnnImmune, ReportsStateExplosion) {
    lnn_network_t* net = create_test_network();
    lnn_immune_bridge_t* bridge = lnn_immune_bridge_create(net, NULL);

    // Inject explosion into state
    for (int i = 0; i < net->layers[0]->n_neurons; i++) {
        net->layers[0]->neurons[i].x = 1e7f;  // Explosion
    }

    lnn_instability_type_t type = lnn_immune_check_stability(bridge);
    EXPECT_EQ(type, LNN_INSTABILITY_STATE_EXPLOSION);

    // Verify immune response
    EXPECT_EQ(lnn_immune_report_instability(bridge, type, 0, 0), 0);
    EXPECT_GT(bridge->instabilities_reported, 0);
}
```

---

## 10. File Manifest

### 10.1 Core LNN Library (22 files, ~8,000 lines)

| File | Lines | Description |
|------|-------|-------------|
| `include/lnn/nimcp_lnn.h` | 200 | Main facade header |
| `include/lnn/nimcp_lnn_types.h` | 400 | Core type definitions |
| `include/lnn/nimcp_lnn_config.h` | 200 | Configuration structures |
| `include/lnn/nimcp_lnn_neuron.h` | 150 | Neuron API |
| `include/lnn/nimcp_lnn_layer.h` | 200 | Layer API |
| `include/lnn/nimcp_lnn_network.h` | 250 | Network API |
| `include/lnn/nimcp_lnn_ode.h` | 150 | ODE solver API |
| `include/lnn/nimcp_lnn_wiring.h` | 200 | Wiring patterns |
| `include/lnn/nimcp_lnn_gradient.h` | 200 | Gradient computation |
| `include/lnn/nimcp_lnn_training.h` | 300 | Training integration |
| `include/lnn/nimcp_lnn_bio_async.h` | 200 | Bio-async integration |
| `include/lnn/nimcp_lnn_immune.h` | 250 | Immune integration |
| `include/lnn/nimcp_lnn_parallel.h` | 200 | Parallelization |
| `src/lnn/nimcp_lnn_neuron.c` | 400 | Neuron implementation |
| `src/lnn/nimcp_lnn_layer.c` | 500 | Layer implementation |
| `src/lnn/nimcp_lnn_network.c` | 600 | Network implementation |
| `src/lnn/nimcp_lnn_ode.c` | 400 | ODE solvers |
| `src/lnn/nimcp_lnn_wiring.c` | 500 | Wiring generators |
| `src/lnn/nimcp_lnn_gradient.c` | 600 | Adjoint method |
| `src/lnn/nimcp_lnn_training.c` | 500 | Training loops |
| `src/lnn/nimcp_lnn_bio_async.c` | 400 | Bio-async handlers |
| `src/lnn/nimcp_lnn_immune.c` | 400 | Immune bridge |
| `src/lnn/nimcp_lnn_parallel.c` | 500 | Parallel execution |

### 10.2 Module Adapters (50 files, ~15,000 lines)

| Module | Header | Source | Tests |
|--------|--------|--------|-------|
| Calcium Dynamics | 150 | 400 | 300 |
| STP | 150 | 400 | 300 |
| Eligibility Traces | 150 | 350 | 250 |
| Homeostatic | 150 | 350 | 250 |
| Second Messengers | 150 | 350 | 250 |
| Neuromodulators | 150 | 350 | 250 |
| Brain Oscillations | 150 | 400 | 300 |
| Cortical Temporal | 150 | 400 | 250 |
| Synchrony Detector | 150 | 300 | 200 |
| Oscillation Detector | 150 | 300 | 200 |
| Predictive Coding | 150 | 400 | 300 |
| Temporal Patterns | 150 | 350 | 250 |
| Sequence Detector | 150 | 350 | 250 |
| Working Memory | 150 | 400 | 300 |
| Sleep-Wake | 150 | 400 | 300 |
| Audio Cortex | 150 | 400 | 300 |
| Speech Cortex | 150 | 400 | 300 |
| Visual Cortex | 150 | 400 | 300 |
| Emotion-Attention | 150 | 350 | 250 |
| Emotional System | 150 | 350 | 250 |
| Thalamic Router | 150 | 300 | 200 |
| Population Coding | 150 | 350 | 250 |
| Temporal Coding | 150 | 300 | 200 |
| Rate Coding | 150 | 300 | 200 |
| Motor/Language | 150 | 350 | 250 |

### 10.3 Test Files (35 files, ~22,000 lines)

| Category | Files | Total Lines |
|----------|-------|-------------|
| Unit Tests - Core | 10 | 4,500 |
| Unit Tests - Adapters | 25 | 7,500 |
| Integration Tests | 10 | 3,000 |
| Regression Tests | 6 | 2,000 |
| E2E Tests | 6 | 2,000 |

### 10.4 Summary

| Category | Files | Lines of Code |
|----------|-------|---------------|
| Core LNN Library | 22 | 8,000 |
| Module Adapters | 50 | 15,000 |
| Test Files | 35 | 22,000 |
| **Total** | **107** | **~45,000** |

---

## Appendix A: Implementation Checklist

### Phase 1: Foundation
- [ ] Create `include/lnn/` directory structure
- [ ] Implement `nimcp_lnn_types.h`
- [ ] Implement `nimcp_lnn_config.h`
- [ ] Implement `nimcp_lnn_neuron.h/.c`
- [ ] Implement `nimcp_lnn_layer.h/.c`
- [ ] Implement `nimcp_lnn_network.h/.c`
- [ ] Implement `nimcp_lnn_ode.h/.c` (Euler, Heun, RK4)
- [ ] Implement `nimcp_lnn_wiring.h/.c` (Full, Random, NCP)
- [ ] Write unit tests for core components (150 tests)
- [ ] Implement calcium dynamics adapter
- [ ] Implement STP adapter
- [ ] Write adapter tests (60 tests)

### Phase 2: Plasticity
- [ ] Implement eligibility trace adapter
- [ ] Implement homeostatic adapter
- [ ] Implement second messenger adapter
- [ ] Implement neuromodulator adapter
- [ ] Write adapter tests (100 tests)

### Phase 3: Oscillations
- [ ] Implement brain oscillations adapter
- [ ] Implement cortical temporal adapter
- [ ] Implement synchrony detector adapter
- [ ] Implement oscillation detector adapter
- [ ] Write adapter tests (95 tests)

### Phase 4: Prediction
- [ ] Implement predictive coding adapter
- [ ] Implement temporal patterns adapter
- [ ] Implement sequence detector adapter
- [ ] Write adapter tests (80 tests)

### Phase 5: Memory
- [ ] Implement working memory adapter
- [ ] Implement sleep-wake adapter
- [ ] Implement circular buffer adapter
- [ ] Write adapter tests (80 tests)

### Phase 6: Sensory
- [ ] Implement audio cortex adapter
- [ ] Implement speech cortex adapter
- [ ] Implement visual cortex adapter
- [ ] Write adapter tests (90 tests)

### Phase 7: Cognition
- [ ] Implement emotion-attention adapter
- [ ] Implement emotional system adapter
- [ ] Implement thalamic router adapter
- [ ] Write adapter tests (70 tests)

### Phase 8: Encoding
- [ ] Implement population coding adapter
- [ ] Implement temporal coding adapter
- [ ] Implement rate coding adapter
- [ ] Implement motor/language adapter
- [ ] Write adapter tests (90 tests)
- [ ] Write integration tests (300 tests)
- [ ] Write regression tests (200 tests)
- [ ] Write E2E tests (100 tests)

---

## Appendix B: Design Patterns Used

| Pattern | Usage |
|---------|-------|
| **Bridge** | Module adapters connecting LNN to NIMCP modules |
| **Strategy** | ODE solvers, activation functions, wiring patterns |
| **Factory** | Network creation with presets (NCP, etc.) |
| **Observer** | Training callbacks, bio-async handlers |
| **State Machine** | Network training mode, state health |
| **Configuration** | All components use config structs with defaults |
| **Pool Allocation** | Pre-allocated neuron arrays in layers |
| **Guard Clauses** | All functions use early returns |
| **WHAT/WHY/HOW** | All functions documented |

---

## Appendix C: Mathematical Utils Integration

| Utility | LNN Usage |
|---------|-----------|
| `nimcp_tensor.h` | State vectors, weight matrices, gradients |
| `nimcp_integration.h` | ODE solvers (Euler, RK4) |
| `nimcp_signal_filter.h` | Low-pass filtering of state for stability |
| `nimcp_complex_math.h` | Phase computation for oscillation sync |
| `nimcp_random.h` | Weight initialization, noise injection |

---

*Document Version: 1.0*
*Created: 2025-12-20*
*Author: NIMCP Development Team*
