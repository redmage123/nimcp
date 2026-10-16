# Variational Autoencoder (VAE) Integration Plan for NIMCP

**Version**: 1.0.0
**Date**: 2026-01-30
**Status**: PROPOSED

---

## Executive Summary

This plan integrates a Variational Autoencoder (VAE) module into NIMCP with deep connections to the Free Energy Principle (FEP) framework and 15+ other cognitive/neural modules. The VAE provides learned latent representations that enable efficient compression, generation, and uncertainty quantification—core capabilities for predictive processing.

**Key Deliverables**:
- Core VAE module with encoder, decoder, and latent space operations
- 16 integration bridges to cognitive, neural, and system modules
- Full immune system, BBB, heartbeat, and resilience integration
- Complete bio-async bidirectional messaging
- SNN/STDP/plasticity training integration
- KG wiring for self-awareness
- 200+ tests (unit, integration, regression, end-to-end)

---

## Table of Contents

1. [Theoretical Foundation](#1-theoretical-foundation)
2. [Architecture Overview](#2-architecture-overview)
3. [Core VAE Module](#3-core-vae-module)
4. [FEP Integration](#4-fep-integration)
5. [Additional Module Integrations](#5-additional-module-integrations)
6. [System Integrations](#6-system-integrations)
7. [Bio-Async Messaging](#7-bio-async-messaging)
8. [SNN/STDP/Plasticity Integration](#8-snnstdpplasticity-integration)
9. [Training Layer Integration](#9-training-layer-integration)
10. [Test Plan](#10-test-plan)
11. [File Manifest](#11-file-manifest)
12. [Implementation Phases](#12-implementation-phases)
13. [Error Codes](#13-error-codes)

---

## 1. Theoretical Foundation

### 1.1 VAE and Free Energy Principle Connection

The Variational Autoencoder is mathematically equivalent to the variational inference framework underlying the Free Energy Principle:

```
VAE Loss = Reconstruction Loss + KL Divergence
         = -E[log p(x|z)] + KL(q(z|x) || p(z))

FEP Free Energy = Inaccuracy + Complexity
                = -E[log p(o|s)] + KL(q(s) || p(s))
```

Where:
- **VAE encoder q(z|x)** ≈ **FEP recognition density q(s)** (belief about hidden states)
- **VAE decoder p(x|z)** ≈ **FEP generative model p(o|s)** (predictions from beliefs)
- **VAE latent z** ≈ **FEP hidden states s** (compressed internal representations)
- **Reconstruction error** ≈ **Prediction error** (sensory surprise)
- **KL divergence** ≈ **Model complexity** (belief deviation from prior)

### 1.2 Why VAE for NIMCP

| Capability | Application in NIMCP |
|------------|---------------------|
| **Learned compression** | Efficient memory storage in hippocampus |
| **Generative sampling** | Imagination, dreaming, counterfactual reasoning |
| **Uncertainty quantification** | Introspection, confidence estimation |
| **Disentangled representations** | Concept separation, transfer learning |
| **Anomaly detection** | Immune system threat detection via reconstruction error |
| **Hierarchical abstraction** | Predictive coding across cortical layers |

### 1.3 References

- Kingma & Welling (2014): Auto-Encoding Variational Bayes
- Friston (2010): The free-energy principle: a unified brain theory?
- Higgins et al. (2017): β-VAE: Learning Basic Visual Concepts
- Rezende & Mohamed (2015): Variational Inference with Normalizing Flows

---

## 2. Architecture Overview

### 2.1 System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              NIMCP VAE System                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐         │
│  │   Input Layer   │───▶│    Encoder      │───▶│  Latent Space   │         │
│  │  (Observations) │    │   q(z|x)        │    │     z ~ N(μ,σ)  │         │
│  └─────────────────┘    └─────────────────┘    └────────┬────────┘         │
│                                                          │                   │
│                         ┌────────────────────────────────┼──────────────┐   │
│                         │              │                 │              │   │
│                         ▼              ▼                 ▼              ▼   │
│                   ┌──────────┐  ┌──────────┐     ┌──────────┐   ┌──────────┐│
│                   │   FEP    │  │Hippocampus│    │ Imagination│  │Introspect││
│                   │ Beliefs  │  │  Memory   │    │  Engine   │  │Uncertainty││
│                   └──────────┘  └──────────┘     └──────────┘   └──────────┘│
│                                                          │                   │
│                                                          ▼                   │
│  ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐         │
│  │  Output Layer   │◀───│    Decoder      │◀───│   Sampling      │         │
│  │ (Reconstructions)│   │   p(x|z)        │    │ Reparameterize  │         │
│  └─────────────────┘    └─────────────────┘    └─────────────────┘         │
│                                                                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                           Integration Layer                                  │
│  ┌─────────┬─────────┬─────────┬─────────┬─────────┬─────────┬─────────┐   │
│  │ Immune  │   BBB   │Bio-Async│   KG    │ Training│   SNN   │Plasticity│  │
│  │ System  │         │ Router  │ Wiring  │  Layer  │  STDP   │         │   │
│  └─────────┴─────────┴─────────┴─────────┴─────────┴─────────┴─────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Module Dependency Graph

```
                              ┌──────────────┐
                              │   VAE Core   │
                              └──────┬───────┘
                                     │
           ┌─────────────────────────┼─────────────────────────┐
           │                         │                         │
           ▼                         ▼                         ▼
    ┌──────────────┐         ┌──────────────┐         ┌──────────────┐
    │  VAE-FEP     │         │  VAE-Memory  │         │ VAE-Perception│
    │   Bridge     │         │   Bridge     │         │    Bridge     │
    └──────┬───────┘         └──────┬───────┘         └──────┬───────┘
           │                         │                         │
           ▼                         ▼                         ▼
    ┌──────────────┐         ┌──────────────┐         ┌──────────────┐
    │ Free Energy  │         │ Hippocampus  │         │Visual Cortex │
    │   System     │         │              │         │Auditory Cortex│
    └──────────────┘         └──────────────┘         └──────────────┘
```

---

## 3. Core VAE Module

### 3.1 Directory Structure

```
include/cognitive/vae/
├── nimcp_vae.h                      # Core VAE system
├── nimcp_vae_encoder.h              # Encoder network
├── nimcp_vae_decoder.h              # Decoder network
├── nimcp_vae_latent.h               # Latent space operations
├── nimcp_vae_loss.h                 # Loss computation (ELBO)
├── nimcp_vae_config.h               # Configuration types
├── bridges/
│   ├── nimcp_vae_fep_bridge.h       # FEP integration
│   ├── nimcp_vae_immune_bridge.h    # Immune system integration
│   ├── nimcp_vae_bbb_bridge.h       # Blood-brain barrier integration
│   ├── nimcp_vae_hippocampus_bridge.h # Memory integration
│   ├── nimcp_vae_imagination_bridge.h # Imagination engine
│   ├── nimcp_vae_visual_bridge.h    # Visual cortex
│   ├── nimcp_vae_auditory_bridge.h  # Auditory cortex
│   ├── nimcp_vae_emotion_bridge.h   # Emotion tensor
│   ├── nimcp_vae_introspection_bridge.h # Uncertainty/introspection
│   ├── nimcp_vae_world_model_bridge.h # World model
│   ├── nimcp_vae_snn_bridge.h       # SNN encoding/decoding
│   ├── nimcp_vae_plasticity_bridge.h # Plasticity/STDP
│   ├── nimcp_vae_training_bridge.h  # Training layer
│   ├── nimcp_vae_substrate_bridge.h # Neural substrate
│   ├── nimcp_vae_thalamic_bridge.h  # Thalamic relay
│   └── nimcp_vae_logging_bridge.h   # Logging integration

src/cognitive/vae/
├── nimcp_vae.c
├── nimcp_vae_encoder.c
├── nimcp_vae_decoder.c
├── nimcp_vae_latent.c
├── nimcp_vae_loss.c
├── bridges/
│   ├── nimcp_vae_fep_bridge.c
│   ├── nimcp_vae_immune_bridge.c
│   ├── nimcp_vae_bbb_bridge.c
│   ├── nimcp_vae_hippocampus_bridge.c
│   ├── nimcp_vae_imagination_bridge.c
│   ├── nimcp_vae_visual_bridge.c
│   ├── nimcp_vae_auditory_bridge.c
│   ├── nimcp_vae_emotion_bridge.c
│   ├── nimcp_vae_introspection_bridge.c
│   ├── nimcp_vae_world_model_bridge.c
│   ├── nimcp_vae_snn_bridge.c
│   ├── nimcp_vae_plasticity_bridge.c
│   ├── nimcp_vae_training_bridge.c
│   ├── nimcp_vae_substrate_bridge.c
│   ├── nimcp_vae_thalamic_bridge.c
│   └── nimcp_vae_logging_bridge.c
```

### 3.2 Core Header: `nimcp_vae.h`

```c
/**
 * @file nimcp_vae.h
 * @brief Variational Autoencoder for NIMCP
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Variational Autoencoder providing learned latent representations
 *       with uncertainty quantification for cognitive processing.
 *
 * WHY:  VAE enables:
 *       - Efficient compression of sensory/cognitive data
 *       - Generative sampling for imagination and dreaming
 *       - Uncertainty estimation via latent variance
 *       - Anomaly detection via reconstruction error
 *       - Direct integration with Free Energy Principle
 *
 * HOW:  Encoder maps inputs to latent distribution parameters (μ, σ).
 *       Reparameterization trick enables gradient flow through sampling.
 *       Decoder reconstructs from latent samples.
 *       ELBO loss = Reconstruction + β*KL divergence.
 *
 * THEORETICAL FOUNDATION:
 *   Kingma & Welling (2014) "Auto-Encoding Variational Bayes"
 *   Higgins et al. (2017) "β-VAE: Learning Basic Visual Concepts"
 *   Friston (2010) "The free-energy principle"
 *
 * ARCHITECTURE:
 *   ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐
 *   │  Input  │───▶│ Encoder │───▶│ Latent  │───▶│ Decoder │───▶ Output
 *   │   x     │    │ q(z|x)  │    │ z~N(μ,σ)│    │ p(x|z)  │
 *   └─────────┘    └─────────┘    └─────────┘    └─────────┘
 *
 * BIO_MODULE: 0x1F00 (VAE Core)
 * BIO_MODULE_VAE_ENCODER: 0x1F01
 * BIO_MODULE_VAE_DECODER: 0x1F02
 * BIO_MODULE_VAE_LATENT:  0x1F03
 * BIO_MODULE_VAE_LOSS:    0x1F04
 */

#ifndef NIMCP_VAE_H
#define NIMCP_VAE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "tensor/nimcp_tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Constants
 *=============================================================================*/

/** Maximum latent dimension */
#define VAE_MAX_LATENT_DIM          1024

/** Maximum input/output dimension */
#define VAE_MAX_IO_DIM              65536

/** Maximum encoder/decoder layers */
#define VAE_MAX_LAYERS              16

/** Default β for β-VAE (KL weight) */
#define VAE_DEFAULT_BETA            1.0f

/** Default learning rate */
#define VAE_DEFAULT_LEARNING_RATE   0.001f

/** Minimum variance to prevent collapse */
#define VAE_MIN_VARIANCE            1e-6f

/** Maximum variance to prevent explosion */
#define VAE_MAX_VARIANCE            100.0f

/** Bio-async module IDs */
#define BIO_MODULE_VAE_CORE         0x1F00
#define BIO_MODULE_VAE_ENCODER      0x1F01
#define BIO_MODULE_VAE_DECODER      0x1F02
#define BIO_MODULE_VAE_LATENT       0x1F03
#define BIO_MODULE_VAE_LOSS         0x1F04
#define BIO_MODULE_VAE_FEP_BRIDGE   0x1F10
#define BIO_MODULE_VAE_IMMUNE_BRIDGE 0x1F11
#define BIO_MODULE_VAE_BBB_BRIDGE   0x1F12

/*=============================================================================
 * Error Codes (32400-32499 range)
 *=============================================================================*/

#define NIMCP_ERROR_VAE_BASE            32400
#define NIMCP_ERROR_VAE_NULL_POINTER    32401
#define NIMCP_ERROR_VAE_INVALID_DIM     32402
#define NIMCP_ERROR_VAE_ENCODER_FAILED  32403
#define NIMCP_ERROR_VAE_DECODER_FAILED  32404
#define NIMCP_ERROR_VAE_LATENT_COLLAPSE 32405
#define NIMCP_ERROR_VAE_VARIANCE_EXPLODE 32406
#define NIMCP_ERROR_VAE_KL_DIVERGE      32407
#define NIMCP_ERROR_VAE_RECON_FAILED    32408
#define NIMCP_ERROR_VAE_GRADIENT_NAN    32409
#define NIMCP_ERROR_VAE_TRAINING_FAILED 32410
#define NIMCP_ERROR_VAE_NO_MEMORY       32411
#define NIMCP_ERROR_VAE_INVALID_CONFIG  32412
#define NIMCP_ERROR_VAE_NOT_INITIALIZED 32413
#define NIMCP_ERROR_VAE_BRIDGE_FAILED   32414
#define NIMCP_ERROR_VAE_BIO_ASYNC_FAILED 32415

/*=============================================================================
 * Enumerations
 *=============================================================================*/

/**
 * @brief VAE operating state
 */
typedef enum {
    VAE_STATE_UNINITIALIZED = 0,    /**< Not yet initialized */
    VAE_STATE_IDLE,                  /**< Initialized, not processing */
    VAE_STATE_ENCODING,              /**< Currently encoding */
    VAE_STATE_SAMPLING,              /**< Currently sampling latent */
    VAE_STATE_DECODING,              /**< Currently decoding */
    VAE_STATE_TRAINING,              /**< In training mode */
    VAE_STATE_ERROR,                 /**< Error state */
    VAE_STATE_RECOVERING             /**< Recovering from error */
} vae_state_t;

/**
 * @brief VAE activation functions
 */
typedef enum {
    VAE_ACTIVATION_RELU = 0,         /**< ReLU activation */
    VAE_ACTIVATION_LEAKY_RELU,       /**< Leaky ReLU */
    VAE_ACTIVATION_ELU,              /**< ELU activation */
    VAE_ACTIVATION_TANH,             /**< Tanh activation */
    VAE_ACTIVATION_SIGMOID,          /**< Sigmoid activation */
    VAE_ACTIVATION_SOFTPLUS,         /**< Softplus (for variance) */
    VAE_ACTIVATION_LINEAR            /**< Linear (identity) */
} vae_activation_t;

/**
 * @brief VAE loss types
 */
typedef enum {
    VAE_LOSS_MSE = 0,                /**< Mean squared error reconstruction */
    VAE_LOSS_BCE,                    /**< Binary cross-entropy (for binary data) */
    VAE_LOSS_GAUSSIAN,               /**< Gaussian negative log-likelihood */
    VAE_LOSS_LAPLACE                 /**< Laplace negative log-likelihood */
} vae_loss_type_t;

/**
 * @brief Latent prior types
 */
typedef enum {
    VAE_PRIOR_STANDARD_NORMAL = 0,   /**< N(0, I) prior */
    VAE_PRIOR_LEARNED,               /**< Learned prior (VampPrior style) */
    VAE_PRIOR_MIXTURE,               /**< Mixture of Gaussians */
    VAE_PRIOR_FLOW                   /**< Normalizing flow prior */
} vae_prior_type_t;

/**
 * @brief VAE variant types
 */
typedef enum {
    VAE_VARIANT_STANDARD = 0,        /**< Standard VAE */
    VAE_VARIANT_BETA,                /**< β-VAE (disentanglement) */
    VAE_VARIANT_CONDITIONAL,         /**< Conditional VAE */
    VAE_VARIANT_HIERARCHICAL,        /**< Hierarchical VAE */
    VAE_VARIANT_VQ                   /**< Vector-quantized VAE */
} vae_variant_t;

/*=============================================================================
 * Configuration Structures
 *=============================================================================*/

/**
 * @brief Layer configuration
 */
typedef struct {
    uint32_t units;                  /**< Number of units */
    vae_activation_t activation;     /**< Activation function */
    float dropout_rate;              /**< Dropout rate (0 = disabled) */
    bool batch_norm;                 /**< Enable batch normalization */
} vae_layer_config_t;

/**
 * @brief Encoder configuration
 */
typedef struct {
    uint32_t input_dim;              /**< Input dimension */
    uint32_t latent_dim;             /**< Latent space dimension */
    uint32_t num_layers;             /**< Number of hidden layers */
    vae_layer_config_t layers[VAE_MAX_LAYERS]; /**< Layer configs */
    vae_activation_t final_activation; /**< Final layer activation */
} vae_encoder_config_t;

/**
 * @brief Decoder configuration
 */
typedef struct {
    uint32_t latent_dim;             /**< Latent space dimension */
    uint32_t output_dim;             /**< Output dimension */
    uint32_t num_layers;             /**< Number of hidden layers */
    vae_layer_config_t layers[VAE_MAX_LAYERS]; /**< Layer configs */
    vae_activation_t final_activation; /**< Final layer activation */
    bool output_variance;            /**< Output variance (heteroscedastic) */
} vae_decoder_config_t;

/**
 * @brief Training configuration
 */
typedef struct {
    float learning_rate;             /**< Learning rate */
    float beta;                      /**< KL divergence weight (β-VAE) */
    float beta_warmup_steps;         /**< Steps to warm up β from 0 */
    vae_loss_type_t loss_type;       /**< Reconstruction loss type */
    float gradient_clip;             /**< Gradient clipping threshold */
    bool free_bits;                  /**< Enable free bits for KL */
    float free_bits_lambda;          /**< Free bits threshold */
    uint32_t batch_size;             /**< Training batch size */
} vae_training_config_t;

/**
 * @brief Main VAE configuration
 */
typedef struct {
    vae_encoder_config_t encoder;    /**< Encoder configuration */
    vae_decoder_config_t decoder;    /**< Decoder configuration */
    vae_training_config_t training;  /**< Training configuration */
    vae_variant_t variant;           /**< VAE variant type */
    vae_prior_type_t prior_type;     /**< Latent prior type */
    bool enable_immune_integration;  /**< Enable immune system integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bool enable_kg_wiring;           /**< Enable KG self-awareness */
    uint32_t heartbeat_interval_ms;  /**< Heartbeat interval */
} vae_config_t;

/*=============================================================================
 * State and Statistics Structures
 *=============================================================================*/

/**
 * @brief Latent space state
 */
typedef struct {
    float* mu;                       /**< Mean vector [latent_dim] */
    float* log_var;                  /**< Log variance [latent_dim] */
    float* z;                        /**< Sampled latent [latent_dim] */
    uint32_t latent_dim;             /**< Latent dimension */
} vae_latent_state_t;

/**
 * @brief Loss components
 */
typedef struct {
    float total_loss;                /**< Total ELBO loss */
    float reconstruction_loss;       /**< Reconstruction term */
    float kl_divergence;             /**< KL divergence term */
    float weighted_kl;               /**< β * KL divergence */
    float free_energy;               /**< Variational free energy (= -ELBO) */
    float inaccuracy;                /**< FEP inaccuracy term */
    float complexity;                /**< FEP complexity term */
} vae_loss_t;

/**
 * @brief VAE statistics
 */
typedef struct {
    uint64_t total_encode_calls;     /**< Total encoding operations */
    uint64_t total_decode_calls;     /**< Total decoding operations */
    uint64_t total_train_steps;      /**< Total training steps */
    float avg_reconstruction_error;  /**< Average reconstruction error */
    float avg_kl_divergence;         /**< Average KL divergence */
    float avg_latent_variance;       /**< Average latent variance */
    float min_latent_variance;       /**< Minimum latent variance (collapse check) */
    float max_latent_variance;       /**< Maximum latent variance (explosion check) */
    uint32_t posterior_collapse_count; /**< Number of collapsed dimensions */
    uint64_t anomalies_detected;     /**< Anomalies via high recon error */
    uint64_t immune_reports;         /**< Reports sent to immune system */
    uint64_t bio_async_messages;     /**< Bio-async messages sent/received */
    float current_beta;              /**< Current β value */
    uint64_t last_update_us;         /**< Last update timestamp */
} vae_stats_t;

/**
 * @brief Health metrics for resilience
 */
typedef struct {
    float encoder_health;            /**< Encoder health (0-1) */
    float decoder_health;            /**< Decoder health (0-1) */
    float latent_health;             /**< Latent space health (0-1) */
    float training_health;           /**< Training stability (0-1) */
    float overall_health;            /**< Overall system health (0-1) */
    bool is_healthy;                 /**< Quick health check */
    uint32_t consecutive_errors;     /**< Consecutive error count */
    uint64_t last_healthy_time_us;   /**< Last healthy timestamp */
} vae_health_t;

/*=============================================================================
 * Main VAE Structure (Opaque)
 *=============================================================================*/

/** Forward declaration */
typedef struct vae_system vae_system_t;

/*=============================================================================
 * Lifecycle API
 *=============================================================================*/

/**
 * @brief Initialize default configuration
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error (throws to immune system)
 */
int vae_default_config(vae_config_t* config);

/**
 * @brief Create VAE system
 * @param config Configuration (NULL for defaults)
 * @return VAE system or NULL on error (throws to immune system)
 */
vae_system_t* vae_create(const vae_config_t* config);

/**
 * @brief Destroy VAE system
 * @param vae VAE system to destroy
 */
void vae_destroy(vae_system_t* vae);

/**
 * @brief Reset VAE to initial state
 * @param vae VAE system
 * @return 0 on success, -1 on error
 */
int vae_reset(vae_system_t* vae);

/*=============================================================================
 * Core Processing API
 *=============================================================================*/

/**
 * @brief Encode input to latent distribution
 * @param vae VAE system
 * @param input Input tensor [batch, input_dim]
 * @param mu Output mean [batch, latent_dim]
 * @param log_var Output log variance [batch, latent_dim]
 * @return 0 on success, -1 on error
 */
int vae_encode(vae_system_t* vae,
               const nimcp_tensor_t* input,
               nimcp_tensor_t* mu,
               nimcp_tensor_t* log_var);

/**
 * @brief Sample from latent distribution (reparameterization trick)
 * @param vae VAE system
 * @param mu Mean tensor [batch, latent_dim]
 * @param log_var Log variance tensor [batch, latent_dim]
 * @param z Output samples [batch, latent_dim]
 * @return 0 on success, -1 on error
 */
int vae_sample(vae_system_t* vae,
               const nimcp_tensor_t* mu,
               const nimcp_tensor_t* log_var,
               nimcp_tensor_t* z);

/**
 * @brief Decode latent to reconstruction
 * @param vae VAE system
 * @param z Latent tensor [batch, latent_dim]
 * @param reconstruction Output reconstruction [batch, output_dim]
 * @return 0 on success, -1 on error
 */
int vae_decode(vae_system_t* vae,
               const nimcp_tensor_t* z,
               nimcp_tensor_t* reconstruction);

/**
 * @brief Full forward pass (encode + sample + decode)
 * @param vae VAE system
 * @param input Input tensor
 * @param reconstruction Output reconstruction
 * @param latent Optional output latent state
 * @return 0 on success, -1 on error
 */
int vae_forward(vae_system_t* vae,
                const nimcp_tensor_t* input,
                nimcp_tensor_t* reconstruction,
                vae_latent_state_t* latent);

/**
 * @brief Compute ELBO loss
 * @param vae VAE system
 * @param input Original input
 * @param reconstruction Reconstructed output
 * @param mu Latent mean
 * @param log_var Latent log variance
 * @param loss Output loss components
 * @return 0 on success, -1 on error
 */
int vae_compute_loss(vae_system_t* vae,
                     const nimcp_tensor_t* input,
                     const nimcp_tensor_t* reconstruction,
                     const nimcp_tensor_t* mu,
                     const nimcp_tensor_t* log_var,
                     vae_loss_t* loss);

/*=============================================================================
 * Training API
 *=============================================================================*/

/**
 * @brief Single training step
 * @param vae VAE system
 * @param input Training input batch
 * @param loss Output loss values
 * @return 0 on success, -1 on error
 */
int vae_train_step(vae_system_t* vae,
                   const nimcp_tensor_t* input,
                   vae_loss_t* loss);

/**
 * @brief Set training mode
 * @param vae VAE system
 * @param training True for training, false for inference
 * @return 0 on success, -1 on error
 */
int vae_set_training(vae_system_t* vae, bool training);

/**
 * @brief Update β value (for β-VAE annealing)
 * @param vae VAE system
 * @param beta New β value
 * @return 0 on success, -1 on error
 */
int vae_set_beta(vae_system_t* vae, float beta);

/*=============================================================================
 * Generation API
 *=============================================================================*/

/**
 * @brief Generate samples from prior
 * @param vae VAE system
 * @param num_samples Number of samples to generate
 * @param samples Output samples [num_samples, output_dim]
 * @return 0 on success, -1 on error
 */
int vae_generate(vae_system_t* vae,
                 uint32_t num_samples,
                 nimcp_tensor_t* samples);

/**
 * @brief Interpolate between two latent points
 * @param vae VAE system
 * @param z1 First latent point
 * @param z2 Second latent point
 * @param num_steps Interpolation steps
 * @param interpolations Output [num_steps, output_dim]
 * @return 0 on success, -1 on error
 */
int vae_interpolate(vae_system_t* vae,
                    const nimcp_tensor_t* z1,
                    const nimcp_tensor_t* z2,
                    uint32_t num_steps,
                    nimcp_tensor_t* interpolations);

/*=============================================================================
 * Query API
 *=============================================================================*/

/**
 * @brief Get current state
 * @param vae VAE system
 * @return Current state
 */
vae_state_t vae_get_state(const vae_system_t* vae);

/**
 * @brief Get statistics
 * @param vae VAE system
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int vae_get_stats(const vae_system_t* vae, vae_stats_t* stats);

/**
 * @brief Get health metrics
 * @param vae VAE system
 * @param health Output health metrics
 * @return 0 on success, -1 on error
 */
int vae_get_health(const vae_system_t* vae, vae_health_t* health);

/**
 * @brief Get current latent state
 * @param vae VAE system
 * @param latent Output latent state
 * @return 0 on success, -1 on error
 */
int vae_get_latent_state(const vae_system_t* vae, vae_latent_state_t* latent);

/**
 * @brief Compute anomaly score (reconstruction error)
 * @param vae VAE system
 * @param input Input to check
 * @param anomaly_score Output anomaly score
 * @return 0 on success, -1 on error
 */
int vae_compute_anomaly_score(vae_system_t* vae,
                              const nimcp_tensor_t* input,
                              float* anomaly_score);

/*=============================================================================
 * Health Agent Integration
 *=============================================================================*/

struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

/**
 * @brief Set health agent for heartbeat reporting
 * @param agent Health agent
 */
void vae_set_health_agent(nimcp_health_agent_t* agent);

/*=============================================================================
 * FEP Integration (Direct API)
 *=============================================================================*/

/**
 * @brief Get variational free energy (negative ELBO)
 * @param vae VAE system
 * @return Free energy value
 */
float vae_get_free_energy(const vae_system_t* vae);

/**
 * @brief Get precision (inverse variance)
 * @param vae VAE system
 * @param precision Output precision vector [latent_dim]
 * @param dim Dimension
 * @return 0 on success, -1 on error
 */
int vae_get_precision(const vae_system_t* vae, float* precision, uint32_t dim);

/**
 * @brief Convert latent to FEP belief state
 * @param vae VAE system
 * @param belief Output belief structure (FEP-compatible)
 * @return 0 on success, -1 on error
 */
int vae_to_fep_belief(const vae_system_t* vae, void* belief);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_H */
```

### 3.3 Core Implementation Patterns

#### Exception Handling (No Silent Returns)

```c
// ALWAYS throw to immune system on error, never return silently
int vae_encode(vae_system_t* vae,
               const nimcp_tensor_t* input,
               nimcp_tensor_t* mu,
               nimcp_tensor_t* log_var) {

    // Null pointer checks with immune reporting
    if (!vae) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "VAE system is NULL in vae_encode");
        return -1;
    }
    if (!input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "Input tensor is NULL in vae_encode");
        return -1;
    }
    if (!mu || !log_var) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "Output tensors are NULL in vae_encode");
        return -1;
    }

    vae_heartbeat("vae_encode", 0.0f);

    nimcp_mutex_lock(vae->mutex);

    // State validation
    if (vae->state == VAE_STATE_UNINITIALIZED) {
        nimcp_mutex_unlock(vae->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NOT_INITIALIZED,
                              "VAE not initialized in vae_encode");
        return -1;
    }

    // Dimension validation
    if (input->dims[input->rank - 1] != vae->config.encoder.input_dim) {
        nimcp_mutex_unlock(vae->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INVALID_DIM,
                              "Input dimension mismatch: expected %u, got %u",
                              vae->config.encoder.input_dim,
                              input->dims[input->rank - 1]);
        return -1;
    }

    vae->state = VAE_STATE_ENCODING;

    // Actual encoding with error detection
    int result = vae_encoder_forward(vae->encoder, input, mu, log_var);
    if (result != 0) {
        vae->state = VAE_STATE_ERROR;
        vae->health.consecutive_errors++;
        nimcp_mutex_unlock(vae->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_ENCODER_FAILED,
                              "Encoder forward pass failed");
        return -1;
    }

    // Variance bounds checking
    float min_var, max_var;
    vae_check_variance_bounds(log_var, &min_var, &max_var);

    if (min_var < logf(VAE_MIN_VARIANCE)) {
        vae->stats.posterior_collapse_count++;
        // Report to immune but don't fail (recoverable)
        if (vae->immune_bridge) {
            vae_immune_report_anomaly(vae->immune_bridge,
                                      VAE_ANOMALY_POSTERIOR_COLLAPSE,
                                      min_var);
        }
        NIMCP_LOGGING_WARN("Potential posterior collapse detected: min_var=%.6f",
                           expf(min_var));
    }

    if (max_var > logf(VAE_MAX_VARIANCE)) {
        vae->state = VAE_STATE_ERROR;
        nimcp_mutex_unlock(vae->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_VARIANCE_EXPLODE,
                              "Variance explosion detected: max_var=%.2f",
                              expf(max_var));
        return -1;
    }

    vae->state = VAE_STATE_IDLE;
    vae->stats.total_encode_calls++;
    vae->health.consecutive_errors = 0;

    nimcp_mutex_unlock(vae->mutex);

    vae_heartbeat("vae_encode", 1.0f);

    return 0;
}
```

---

## 4. FEP Integration

### 4.1 VAE-FEP Bridge Header: `nimcp_vae_fep_bridge.h`

```c
/**
 * @file nimcp_vae_fep_bridge.h
 * @brief Bridge between VAE and Free Energy Principle systems
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Bidirectional integration between VAE latent space and FEP beliefs
 *
 * WHY:  VAE provides learned recognition/generative models that can serve as
 *       the implementation substrate for FEP's variational inference.
 *       - VAE encoder = FEP recognition density q(s|o)
 *       - VAE decoder = FEP generative model p(o|s)
 *       - VAE latent = FEP hidden state beliefs
 *       - VAE ELBO = FEP variational free energy
 *
 * HOW:  Bridge synchronizes:
 *       VAE → FEP: Latent μ/σ become belief means/precisions
 *       FEP → VAE: Prediction errors guide VAE training
 *       Bidirectional: Free energy computation shared
 *
 * MAPPING:
 *   VAE Component        | FEP Component
 *   ---------------------|--------------------
 *   q(z|x)               | Recognition density q(s|o)
 *   p(x|z)               | Generative model p(o|s)
 *   z ~ N(μ, σ²)         | Posterior belief about s
 *   Reconstruction error | Prediction error (inaccuracy)
 *   KL(q||p)             | Model complexity
 *   -ELBO                | Variational free energy
 *   1/σ²                 | Precision weighting
 *
 * BIO_MODULE: 0x1F10 (VAE-FEP Bridge)
 */

#ifndef NIMCP_VAE_FEP_BRIDGE_H
#define NIMCP_VAE_FEP_BRIDGE_H

#include "cognitive/vae/nimcp_vae.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/bridge/nimcp_bridge_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Constants
 *=============================================================================*/

#define VAE_FEP_BRIDGE_VERSION          "1.0.0"
#define VAE_FEP_MAX_HIERARCHY_LEVELS    8

/*=============================================================================
 * Configuration
 *=============================================================================*/

/**
 * @brief VAE-FEP bridge configuration
 */
typedef struct {
    /* Synchronization settings */
    bool sync_latent_to_belief;      /**< Sync VAE latent → FEP beliefs */
    bool sync_belief_to_latent;      /**< Sync FEP beliefs → VAE latent */
    bool share_free_energy;          /**< Share free energy computation */

    /* Mapping parameters */
    float precision_scale;           /**< Scale factor for precision mapping */
    float belief_update_rate;        /**< Rate of belief updates */
    float prediction_error_threshold; /**< Threshold for error reporting */

    /* Hierarchical settings */
    bool enable_hierarchical;        /**< Enable hierarchical VAE-FEP */
    uint32_t num_hierarchy_levels;   /**< Number of hierarchy levels */

    /* Integration settings */
    bool enable_active_inference;    /**< Enable active inference through VAE */
    bool protect_core_weights;       /**< Protect critical VAE weights */

    /* Bio-async settings */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    uint32_t message_buffer_size;    /**< Message buffer size */
} vae_fep_bridge_config_t;

/*=============================================================================
 * State Structures
 *=============================================================================*/

/**
 * @brief Mapping state between VAE and FEP
 */
typedef struct {
    float* latent_to_belief_map;     /**< Mapping from VAE z to FEP s */
    float* belief_to_latent_map;     /**< Mapping from FEP s to VAE z */
    float* precision_weights;        /**< Current precision weights */
    float shared_free_energy;        /**< Shared free energy value */
    float inaccuracy;                /**< Shared inaccuracy (recon error) */
    float complexity;                /**< Shared complexity (KL) */
    uint64_t last_sync_us;           /**< Last synchronization time */
} vae_fep_mapping_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_syncs;            /**< Total synchronizations */
    uint64_t latent_to_belief_syncs; /**< VAE → FEP syncs */
    uint64_t belief_to_latent_syncs; /**< FEP → VAE syncs */
    uint64_t prediction_errors_sent; /**< Errors sent to FEP */
    uint64_t belief_updates_received; /**< Updates from FEP */
    float avg_sync_latency_us;       /**< Average sync latency */
    float avg_free_energy;           /**< Average free energy */
    float avg_precision;             /**< Average precision */
} vae_fep_bridge_stats_t;

/*=============================================================================
 * Bridge Structure
 *=============================================================================*/

/**
 * @brief VAE-FEP bridge (opaque)
 */
typedef struct vae_fep_bridge {
    bridge_base_t base;              /**< Base bridge (MUST be first) */
    vae_fep_bridge_config_t config;
    vae_system_t* vae;
    fep_system_t* fep;
    vae_fep_mapping_state_t mapping;
    vae_fep_bridge_stats_t stats;
    nimcp_mutex_t* mutex;
    bool is_connected;
} vae_fep_bridge_t;

/*=============================================================================
 * Lifecycle API
 *=============================================================================*/

int vae_fep_bridge_default_config(vae_fep_bridge_config_t* config);
vae_fep_bridge_t* vae_fep_bridge_create(const vae_fep_bridge_config_t* config);
void vae_fep_bridge_destroy(vae_fep_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *=============================================================================*/

int vae_fep_bridge_connect_vae(vae_fep_bridge_t* bridge, vae_system_t* vae);
int vae_fep_bridge_connect_fep(vae_fep_bridge_t* bridge, fep_system_t* fep);
int vae_fep_bridge_disconnect(vae_fep_bridge_t* bridge);
bool vae_fep_bridge_is_connected(const vae_fep_bridge_t* bridge);

/*=============================================================================
 * Synchronization API
 *=============================================================================*/

/**
 * @brief Sync VAE latent state to FEP beliefs
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int vae_fep_sync_latent_to_belief(vae_fep_bridge_t* bridge);

/**
 * @brief Sync FEP beliefs to VAE latent state
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int vae_fep_sync_belief_to_latent(vae_fep_bridge_t* bridge);

/**
 * @brief Full bidirectional synchronization
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int vae_fep_bridge_sync(vae_fep_bridge_t* bridge);

/*=============================================================================
 * Free Energy API
 *=============================================================================*/

/**
 * @brief Compute shared free energy from VAE ELBO
 * @param bridge Bridge instance
 * @param free_energy Output free energy
 * @return 0 on success, -1 on error
 */
int vae_fep_compute_free_energy(vae_fep_bridge_t* bridge, float* free_energy);

/**
 * @brief Get precision weights from VAE latent variance
 * @param bridge Bridge instance
 * @param precision Output precision array
 * @param dim Dimension
 * @return 0 on success, -1 on error
 */
int vae_fep_get_precision(vae_fep_bridge_t* bridge, float* precision, uint32_t dim);

/**
 * @brief Report prediction error to FEP
 * @param bridge Bridge instance
 * @param error_magnitude Error magnitude
 * @return 0 on success, -1 on error
 */
int vae_fep_report_prediction_error(vae_fep_bridge_t* bridge, float error_magnitude);

/*=============================================================================
 * Active Inference API
 *=============================================================================*/

/**
 * @brief Compute expected free energy for action
 * @param bridge Bridge instance
 * @param action_latent Action in latent space
 * @param expected_fe Output expected free energy
 * @return 0 on success, -1 on error
 */
int vae_fep_compute_expected_free_energy(vae_fep_bridge_t* bridge,
                                         const nimcp_tensor_t* action_latent,
                                         float* expected_fe);

/*=============================================================================
 * Update API
 *=============================================================================*/

/**
 * @brief Main bridge update (call each cycle)
 * @param bridge Bridge instance
 * @param delta_ms Time since last update
 * @return 0 on success, -1 on error
 */
int vae_fep_bridge_update(vae_fep_bridge_t* bridge, uint64_t delta_ms);

/*=============================================================================
 * Query API
 *=============================================================================*/

int vae_fep_bridge_get_stats(const vae_fep_bridge_t* bridge,
                             vae_fep_bridge_stats_t* stats);
int vae_fep_bridge_get_mapping(const vae_fep_bridge_t* bridge,
                               vae_fep_mapping_state_t* mapping);

/*=============================================================================
 * Bio-Async Integration
 *=============================================================================*/

int vae_fep_bridge_connect_bio_async(vae_fep_bridge_t* bridge);
int vae_fep_bridge_disconnect_bio_async(vae_fep_bridge_t* bridge);
bool vae_fep_bridge_is_bio_async_connected(const vae_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_FEP_BRIDGE_H */
```

---

## 5. Additional Module Integrations

### 5.1 Modules That Benefit from VAE

| Module | Integration Type | VAE Application |
|--------|-----------------|-----------------|
| **Hippocampus** | Memory compression | Encode memories to latent, decode for recall |
| **Imagination Engine** | Generative sampling | Sample from latent for imagination |
| **Visual Cortex** | Perceptual encoding | Hierarchical visual VAE |
| **Auditory Cortex** | Auditory encoding | Spectral/temporal VAE |
| **Emotion Tensor** | Emotion compression | Latent emotion space |
| **Introspection** | Uncertainty estimation | Latent variance → confidence |
| **World Model** | State compression | World state in latent space |
| **Predictive Coding** | Hierarchical prediction | Multi-level VAE |
| **Sleep System** | Consolidation/replay | VAE-based replay generation |
| **Attention** | Salience computation | Reconstruction error → salience |

### 5.2 Bridge Summary Table

| Bridge | Header | BIO_MODULE | Primary Function |
|--------|--------|------------|------------------|
| VAE-FEP | `nimcp_vae_fep_bridge.h` | 0x1F10 | Belief/free energy sync |
| VAE-Immune | `nimcp_vae_immune_bridge.h` | 0x1F11 | Anomaly reporting |
| VAE-BBB | `nimcp_vae_bbb_bridge.h` | 0x1F12 | Permeability filtering |
| VAE-Hippocampus | `nimcp_vae_hippocampus_bridge.h` | 0x1F13 | Memory encoding |
| VAE-Imagination | `nimcp_vae_imagination_bridge.h` | 0x1F14 | Generative sampling |
| VAE-Visual | `nimcp_vae_visual_bridge.h` | 0x1F15 | Visual encoding |
| VAE-Auditory | `nimcp_vae_auditory_bridge.h` | 0x1F16 | Auditory encoding |
| VAE-Emotion | `nimcp_vae_emotion_bridge.h` | 0x1F17 | Emotion latent space |
| VAE-Introspection | `nimcp_vae_introspection_bridge.h` | 0x1F18 | Uncertainty estimation |
| VAE-WorldModel | `nimcp_vae_world_model_bridge.h` | 0x1F19 | State compression |
| VAE-SNN | `nimcp_vae_snn_bridge.h` | 0x1F1A | Spike encoding |
| VAE-Plasticity | `nimcp_vae_plasticity_bridge.h` | 0x1F1B | Synaptic learning |
| VAE-Training | `nimcp_vae_training_bridge.h` | 0x1F1C | Training callbacks |
| VAE-Substrate | `nimcp_vae_substrate_bridge.h` | 0x1F1D | Neural substrate |
| VAE-Thalamic | `nimcp_vae_thalamic_bridge.h` | 0x1F1E | Relay gating |
| VAE-Logging | `nimcp_vae_logging_bridge.h` | 0x1F1F | Structured logging |

---

## 6. System Integrations

### 6.1 Immune System Integration

```c
/**
 * @file nimcp_vae_immune_bridge.h
 * @brief VAE-Immune system bidirectional integration
 *
 * VAE → Immune:
 *   - High reconstruction error → Antigen presentation
 *   - Posterior collapse → Health warning
 *   - Training instability → System alert
 *
 * Immune → VAE:
 *   - Inflammation → Reduced learning rate
 *   - Cytokine effects → Precision modulation
 *   - Recovery signals → Parameter restoration
 */

/* Anomaly types reported to immune */
typedef enum {
    VAE_ANOMALY_HIGH_RECON_ERROR = 0, /**< Reconstruction error > threshold */
    VAE_ANOMALY_POSTERIOR_COLLAPSE,   /**< Latent dimension collapsed */
    VAE_ANOMALY_VARIANCE_EXPLOSION,   /**< Variance grew too large */
    VAE_ANOMALY_KL_DIVERGENCE,        /**< KL divergence abnormal */
    VAE_ANOMALY_GRADIENT_NAN,         /**< NaN in gradients */
    VAE_ANOMALY_TRAINING_UNSTABLE     /**< Training loss oscillating */
} vae_anomaly_type_t;

/* Immune effects on VAE */
typedef struct {
    float learning_rate_modifier;    /**< 0.0-1.0, multiplied with LR */
    float precision_modifier;        /**< 0.0-1.0, affects precision */
    float beta_modifier;             /**< Modifier for β-VAE */
    bool training_inhibited;         /**< Training disabled by immune */
    bool in_recovery_mode;           /**< System in recovery */
} vae_immune_effects_t;

/* API */
int vae_immune_bridge_report_anomaly(vae_immune_bridge_t* bridge,
                                     vae_anomaly_type_t type,
                                     float magnitude);
int vae_immune_bridge_get_effects(vae_immune_bridge_t* bridge,
                                  vae_immune_effects_t* effects);
int vae_immune_bridge_apply_effects(vae_immune_bridge_t* bridge);
```

### 6.2 BBB Integration

```c
/**
 * @file nimcp_vae_bbb_bridge.h
 * @brief VAE-Blood Brain Barrier integration
 *
 * BBB filters what enters/exits the VAE latent space:
 *   - Permeability affects latent information flow
 *   - Inflammation reduces latent bandwidth
 *   - Protection against adversarial latent perturbations
 */

typedef struct {
    float latent_permeability;       /**< 0.0-1.0, latent flow rate */
    float input_gate;                /**< Input gating factor */
    float output_gate;               /**< Output gating factor */
    bool is_compromised;             /**< BBB integrity compromised */
} vae_bbb_state_t;
```

### 6.3 Heartbeat and Resilience

```c
// In every function, call heartbeat with operation name and progress
static inline void vae_heartbeat(const char* operation, float progress) {
    if (g_vae_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_vae_health_agent, operation, progress);
    }
}

// Usage in long operations
int vae_train_epoch(vae_system_t* vae, const nimcp_tensor_t* data) {
    uint32_t num_batches = data->dims[0] / vae->config.training.batch_size;

    for (uint32_t i = 0; i < num_batches; i++) {
        // Heartbeat every batch
        vae_heartbeat("vae_train_epoch", (float)(i+1) / (float)num_batches);

        // Training step
        int result = vae_train_step(vae, batch, &loss);
        if (result != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_TRAINING_FAILED,
                                  "Training step %u failed", i);
            return -1;
        }
    }
    return 0;
}
```

### 6.4 KG Wiring

```c
/**
 * Register VAE with Knowledge Graph for self-awareness
 */
int vae_register_kg(vae_system_t* vae) {
    if (!vae || !vae->kg_reader) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "Cannot register NULL VAE with KG");
        return -1;
    }

    // Register as entity
    kg_entity_t entity = {
        .name = "VAE_System",
        .type = "CognitiveModule",
        .observations = {
            "Provides variational autoencoding for latent representations",
            "Integrates with FEP for belief/precision computation",
            "Supports generative sampling for imagination",
            "Detects anomalies via reconstruction error"
        }
    };

    kg_reader_add_entity(vae->kg_reader, &entity);

    // Register relations
    kg_reader_add_relation(vae->kg_reader, "VAE_System", "provides_to", "FEP_System");
    kg_reader_add_relation(vae->kg_reader, "VAE_System", "reports_to", "Immune_System");
    kg_reader_add_relation(vae->kg_reader, "VAE_System", "encodes_for", "Hippocampus");

    return 0;
}
```

### 6.5 Logging Integration

```c
/**
 * @file nimcp_vae_logging_bridge.h
 * @brief Structured logging for VAE operations
 */

#define LOG_MODULE "VAE"

/* Log levels for different events */
#define VAE_LOG_ENCODE_START     NIMCP_LOGGING_DEBUG
#define VAE_LOG_ENCODE_COMPLETE  NIMCP_LOGGING_DEBUG
#define VAE_LOG_TRAIN_STEP       NIMCP_LOGGING_INFO
#define VAE_LOG_LOSS_VALUES      NIMCP_LOGGING_INFO
#define VAE_LOG_ANOMALY          NIMCP_LOGGING_WARN
#define VAE_LOG_ERROR            NIMCP_LOGGING_ERROR

/* Structured log with context */
typedef struct {
    const char* operation;
    float loss;
    float kl;
    float recon;
    uint32_t step;
    uint64_t latency_us;
} vae_log_context_t;

int vae_log_operation(const vae_log_context_t* ctx);
```

---

## 7. Bio-Async Messaging

### 7.1 Message Types

```c
/* Bio-async message types for VAE (0x1F00 range) */
typedef enum {
    /* VAE Core Messages */
    BIO_MSG_VAE_ENCODE_REQUEST      = 0x1F00,
    BIO_MSG_VAE_ENCODE_RESPONSE     = 0x1F01,
    BIO_MSG_VAE_DECODE_REQUEST      = 0x1F02,
    BIO_MSG_VAE_DECODE_RESPONSE     = 0x1F03,
    BIO_MSG_VAE_SAMPLE_REQUEST      = 0x1F04,
    BIO_MSG_VAE_SAMPLE_RESPONSE     = 0x1F05,

    /* VAE-FEP Messages */
    BIO_MSG_VAE_LATENT_UPDATE       = 0x1F10,
    BIO_MSG_VAE_BELIEF_SYNC         = 0x1F11,
    BIO_MSG_VAE_FREE_ENERGY         = 0x1F12,
    BIO_MSG_VAE_PRECISION_UPDATE    = 0x1F13,
    BIO_MSG_VAE_PREDICTION_ERROR    = 0x1F14,

    /* VAE-Immune Messages */
    BIO_MSG_VAE_ANOMALY_REPORT      = 0x1F20,
    BIO_MSG_VAE_HEALTH_STATUS       = 0x1F21,
    BIO_MSG_VAE_RECOVERY_REQUEST    = 0x1F22,

    /* VAE-Memory Messages */
    BIO_MSG_VAE_MEMORY_ENCODE       = 0x1F30,
    BIO_MSG_VAE_MEMORY_DECODE       = 0x1F31,
    BIO_MSG_VAE_MEMORY_CONSOLIDATE  = 0x1F32,

    /* VAE-Imagination Messages */
    BIO_MSG_VAE_GENERATE_REQUEST    = 0x1F40,
    BIO_MSG_VAE_INTERPOLATE_REQUEST = 0x1F41,
    BIO_MSG_VAE_IMAGINE_SCENE       = 0x1F42,

    /* VAE-Training Messages */
    BIO_MSG_VAE_TRAIN_STEP          = 0x1F50,
    BIO_MSG_VAE_GRADIENT_UPDATE     = 0x1F51,
    BIO_MSG_VAE_WEIGHT_CHECKPOINT   = 0x1F52
} vae_bio_message_t;
```

### 7.2 Message Structures

```c
/* Latent update message */
typedef struct {
    uint32_t latent_dim;
    float mu[VAE_MAX_LATENT_DIM];
    float log_var[VAE_MAX_LATENT_DIM];
    uint64_t timestamp_us;
} vae_latent_update_msg_t;

/* Anomaly report message */
typedef struct {
    vae_anomaly_type_t type;
    float magnitude;
    float reconstruction_error;
    float kl_divergence;
    uint64_t timestamp_us;
    char context[128];
} vae_anomaly_report_msg_t;

/* Free energy message */
typedef struct {
    float total_free_energy;
    float inaccuracy;
    float complexity;
    float beta;
    uint64_t timestamp_us;
} vae_free_energy_msg_t;
```

### 7.3 Message Handlers

```c
/* Register VAE message handlers with bio-async router */
int vae_register_bio_async_handlers(vae_system_t* vae) {
    if (!vae) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "Cannot register NULL VAE handlers");
        return -1;
    }

    int result;

    // Register encode request handler
    result = bio_router_register_handler(
        BIO_MSG_VAE_ENCODE_REQUEST,
        vae_handle_encode_request,
        vae
    );
    if (result != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_BIO_ASYNC_FAILED,
                              "Failed to register encode handler");
        return -1;
    }

    // Register decode request handler
    result = bio_router_register_handler(
        BIO_MSG_VAE_DECODE_REQUEST,
        vae_handle_decode_request,
        vae
    );
    if (result != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_BIO_ASYNC_FAILED,
                              "Failed to register decode handler");
        return -1;
    }

    // ... register all handlers ...

    vae->bio_async_connected = true;
    return 0;
}

/* Example handler implementation */
static nimcp_error_t vae_handle_encode_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    vae_system_t* vae = (vae_system_t*)user_data;

    if (!vae || !msg) {
        // Cannot throw here, but return error code
        return NIMCP_ERROR_VAE_NULL_POINTER;
    }

    vae_encode_request_msg_t* request = (vae_encode_request_msg_t*)msg;

    // Perform encoding
    nimcp_tensor_t* mu = nimcp_tensor_create_1d(vae->config.encoder.latent_dim,
                                                NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create_1d(vae->config.encoder.latent_dim,
                                                     NIMCP_DTYPE_FLOAT32);

    int result = vae_encode(vae, request->input, mu, log_var);

    // Send response
    vae_encode_response_msg_t response = {
        .success = (result == 0),
        .latent_dim = vae->config.encoder.latent_dim,
        .timestamp_us = nimcp_get_timestamp_us()
    };

    if (result == 0) {
        memcpy(response.mu, mu->data, mu->size);
        memcpy(response.log_var, log_var->data, log_var->size);
    }

    bio_promise_resolve(response_promise, &response, sizeof(response));

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);

    return result == 0 ? NIMCP_SUCCESS : NIMCP_ERROR_VAE_ENCODER_FAILED;
}
```

---

## 8. SNN/STDP/Plasticity Integration

### 8.1 VAE-SNN Bridge

```c
/**
 * @file nimcp_vae_snn_bridge.h
 * @brief Bridge between VAE and Spiking Neural Networks
 *
 * WHAT: Encode/decode between continuous VAE latent space and spike trains
 *
 * WHY:  Enable VAE integration with SNN-based computation:
 *       - Rate coding: Latent value → spike rate
 *       - Population coding: Latent → distributed spikes
 *       - Temporal coding: Latent → spike timing
 *
 * HOW:  Bidirectional conversion:
 *       VAE latent → Spike encoder → SNN input
 *       SNN output → Spike decoder → VAE latent
 */

/* Spike encoding types */
typedef enum {
    VAE_SNN_ENCODING_RATE = 0,       /**< Rate coding (Poisson) */
    VAE_SNN_ENCODING_POPULATION,     /**< Population coding */
    VAE_SNN_ENCODING_TEMPORAL,       /**< Temporal/phase coding */
    VAE_SNN_ENCODING_RANK_ORDER      /**< Rank order coding */
} vae_snn_encoding_t;

/* Configuration */
typedef struct {
    vae_snn_encoding_t encoding_type;
    uint32_t neurons_per_latent;     /**< SNN neurons per latent dim */
    float max_spike_rate;            /**< Maximum spike rate (Hz) */
    float time_window_ms;            /**< Integration time window */
    bool enable_stdp;                /**< Enable STDP on VAE→SNN synapses */
} vae_snn_bridge_config_t;

/* API */
int vae_snn_encode_latent(vae_snn_bridge_t* bridge,
                          const float* latent,
                          uint32_t latent_dim,
                          spike_train_t* spikes);

int vae_snn_decode_spikes(vae_snn_bridge_t* bridge,
                          const spike_train_t* spikes,
                          float* latent,
                          uint32_t latent_dim);
```

### 8.2 VAE-Plasticity Bridge

```c
/**
 * @file nimcp_vae_plasticity_bridge.h
 * @brief Bridge between VAE and synaptic plasticity
 *
 * WHAT: Connect VAE learning to STDP and other plasticity mechanisms
 *
 * WHY:  VAE training can be mapped to biologically plausible learning:
 *       - Reconstruction error → Prediction error → LTD/LTP
 *       - KL divergence → Prior regularization → Homeostatic plasticity
 *       - β-VAE → Sparsity → Competition
 *
 * HOW:  VAE gradients converted to eligibility traces
 *       STDP rules modulated by VAE loss components
 */

/* Plasticity event types */
typedef enum {
    VAE_PLAST_RECON_ERROR_LOW = 0,   /**< Good reconstruction → LTP */
    VAE_PLAST_RECON_ERROR_HIGH,      /**< Bad reconstruction → LTD */
    VAE_PLAST_KL_INCREASE,           /**< KL increased → Regularize */
    VAE_PLAST_KL_DECREASE,           /**< KL decreased → Consolidate */
    VAE_PLAST_LATENT_ACTIVE,         /**< Latent dimension active */
    VAE_PLAST_LATENT_INACTIVE        /**< Latent dimension inactive (prune?) */
} vae_plasticity_event_t;

/* Weight protection */
typedef struct {
    bool protect_encoder_input;      /**< Protect first encoder layer */
    bool protect_decoder_output;     /**< Protect last decoder layer */
    float min_weight;                /**< Minimum weight bound */
    float max_weight;                /**< Maximum weight bound */
} vae_weight_protection_t;

/* API */
int vae_plasticity_report_event(vae_plasticity_bridge_t* bridge,
                                vae_plasticity_event_t event,
                                float magnitude);

int vae_plasticity_apply_stdp(vae_plasticity_bridge_t* bridge,
                              const spike_train_t* pre,
                              const spike_train_t* post);

int vae_plasticity_get_eligibility(vae_plasticity_bridge_t* bridge,
                                   float* eligibility,
                                   uint32_t size);
```

---

## 9. Training Layer Integration

### 9.1 VAE-Training Bridge

```c
/**
 * @file nimcp_vae_training_bridge.h
 * @brief Bridge between VAE and NIMCP training layer
 *
 * Integrates VAE training with:
 *   - Training callbacks (loss, gradient, checkpoint events)
 *   - Optimizer integration
 *   - Learning rate scheduling
 *   - Gradient accumulation
 *   - Mixed precision support
 */

/* Training callback events specific to VAE */
typedef enum {
    VAE_TCB_ELBO_COMPUTED = 0,       /**< ELBO loss computed */
    VAE_TCB_RECON_COMPUTED,          /**< Reconstruction loss computed */
    VAE_TCB_KL_COMPUTED,             /**< KL divergence computed */
    VAE_TCB_BETA_UPDATED,            /**< β value updated */
    VAE_TCB_POSTERIOR_COLLAPSE,      /**< Posterior collapse detected */
    VAE_TCB_GRADIENT_COMPUTED,       /**< Gradients computed */
    VAE_TCB_WEIGHTS_UPDATED,         /**< Weights updated */
    VAE_TCB_CHECKPOINT_SAVED         /**< Model checkpoint saved */
} vae_training_callback_event_t;

/* Training state */
typedef struct {
    uint64_t global_step;
    uint64_t epoch;
    float current_lr;
    float current_beta;
    float ema_loss;
    float ema_recon;
    float ema_kl;
    bool is_training;
    bool warmup_complete;
} vae_training_state_t;

/* Callback signature */
typedef void (*vae_training_callback_fn)(
    vae_training_callback_event_t event,
    const vae_training_state_t* state,
    const vae_loss_t* loss,
    void* user_data
);

/* API */
int vae_training_register_callback(vae_training_bridge_t* bridge,
                                   vae_training_callback_event_t event,
                                   vae_training_callback_fn callback,
                                   void* user_data);

int vae_training_set_optimizer(vae_training_bridge_t* bridge,
                               optimizer_t* optimizer);

int vae_training_set_lr_scheduler(vae_training_bridge_t* bridge,
                                  lr_scheduler_t* scheduler);

int vae_training_checkpoint(vae_training_bridge_t* bridge,
                            const char* path);

int vae_training_restore(vae_training_bridge_t* bridge,
                         const char* path);
```

---

## 10. Test Plan

### 10.1 Test Structure

```
tests/cognitive/vae/
├── unit/
│   ├── test_vae_core.cpp                    # Core VAE functionality
│   ├── test_vae_encoder.cpp                 # Encoder tests
│   ├── test_vae_decoder.cpp                 # Decoder tests
│   ├── test_vae_latent.cpp                  # Latent space operations
│   ├── test_vae_loss.cpp                    # Loss computation
│   ├── test_vae_reparameterization.cpp      # Reparameterization trick
│   ├── test_vae_config.cpp                  # Configuration validation
│   └── test_vae_generation.cpp              # Generation/sampling
│
├── integration/
│   ├── test_vae_fep_integration.cpp         # VAE-FEP integration
│   ├── test_vae_immune_integration.cpp      # VAE-Immune integration
│   ├── test_vae_bbb_integration.cpp         # VAE-BBB integration
│   ├── test_vae_hippocampus_integration.cpp # VAE-Memory integration
│   ├── test_vae_imagination_integration.cpp # VAE-Imagination integration
│   ├── test_vae_visual_integration.cpp      # VAE-Visual integration
│   ├── test_vae_emotion_integration.cpp     # VAE-Emotion integration
│   ├── test_vae_introspection_integration.cpp # VAE-Introspection
│   ├── test_vae_snn_integration.cpp         # VAE-SNN integration
│   ├── test_vae_plasticity_integration.cpp  # VAE-Plasticity integration
│   ├── test_vae_training_integration.cpp    # VAE-Training integration
│   ├── test_vae_bio_async_integration.cpp   # Bio-async messaging
│   ├── test_vae_kg_integration.cpp          # KG wiring
│   └── test_vae_logging_integration.cpp     # Logging integration
│
├── regression/
│   ├── test_vae_numerical_stability.cpp     # Numerical stability
│   ├── test_vae_gradient_flow.cpp           # Gradient flow
│   ├── test_vae_posterior_collapse.cpp      # Posterior collapse prevention
│   ├── test_vae_variance_bounds.cpp         # Variance bounds
│   ├── test_vae_memory_leaks.cpp            # Memory leak detection
│   ├── test_vae_thread_safety.cpp           # Thread safety
│   └── test_vae_error_handling.cpp          # Error handling coverage
│
└── e2e/
    ├── e2e_test_vae_fep_pipeline.cpp        # Full VAE-FEP pipeline
    ├── e2e_test_vae_memory_consolidation.cpp # Memory encoding/recall
    ├── e2e_test_vae_imagination_generation.cpp # Imagination generation
    ├── e2e_test_vae_anomaly_detection.cpp   # Anomaly detection flow
    ├── e2e_test_vae_training_convergence.cpp # Training convergence
    └── e2e_test_vae_resilience.cpp          # Fault recovery
```

### 10.2 Test Counts by Category

| Category | Test Files | Tests per File | Total Tests |
|----------|-----------|----------------|-------------|
| Unit | 8 | ~15 | ~120 |
| Integration | 15 | ~8 | ~120 |
| Regression | 7 | ~10 | ~70 |
| End-to-End | 6 | ~5 | ~30 |
| **Total** | **36** | - | **~340** |

### 10.3 Critical Test Cases

#### Unit Tests

```cpp
// test_vae_core.cpp
TEST(VAECore, CreateDestroy) {
    vae_config_t config;
    ASSERT_EQ(vae_default_config(&config), 0);

    vae_system_t* vae = vae_create(&config);
    ASSERT_NE(vae, nullptr);

    vae_destroy(vae);
}

TEST(VAECore, NullPointerThrowsToImmune) {
    // This should throw to immune, not silently return
    int result = vae_encode(NULL, NULL, NULL, NULL);
    ASSERT_EQ(result, -1);
    // Verify immune system received the report
    ASSERT_GT(get_immune_report_count(), 0);
}

TEST(VAECore, ForwardPassDimensions) {
    vae_system_t* vae = create_test_vae(64, 32, 64); // in, latent, out

    nimcp_tensor_t* input = nimcp_tensor_create_2d(16, 64, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* recon = nimcp_tensor_create_2d(16, 64, NIMCP_DTYPE_FLOAT32);

    vae_latent_state_t latent;
    ASSERT_EQ(vae_forward(vae, input, recon, &latent), 0);

    ASSERT_EQ(latent.latent_dim, 32);

    cleanup(vae, input, recon);
}

// test_vae_loss.cpp
TEST(VAELoss, ELBOComputation) {
    vae_loss_t loss;
    int result = vae_compute_loss(vae, input, recon, mu, log_var, &loss);

    ASSERT_EQ(result, 0);
    ASSERT_GE(loss.reconstruction_loss, 0.0f);
    ASSERT_GE(loss.kl_divergence, 0.0f);
    ASSERT_FLOAT_EQ(loss.total_loss,
                    loss.reconstruction_loss + loss.weighted_kl);
}

TEST(VAELoss, FreeEnergyEquivalence) {
    // VAE -ELBO should equal FEP free energy
    vae_loss_t loss;
    vae_compute_loss(vae, input, recon, mu, log_var, &loss);

    ASSERT_FLOAT_EQ(loss.free_energy, -(-loss.reconstruction_loss - loss.kl_divergence));
    ASSERT_FLOAT_EQ(loss.inaccuracy, loss.reconstruction_loss);
    ASSERT_FLOAT_EQ(loss.complexity, loss.kl_divergence);
}
```

#### Integration Tests

```cpp
// test_vae_fep_integration.cpp
TEST(VAEFEPIntegration, LatentToBeliefSync) {
    vae_system_t* vae = create_test_vae();
    fep_system_t* fep = create_test_fep();
    vae_fep_bridge_t* bridge = vae_fep_bridge_create(NULL);

    ASSERT_EQ(vae_fep_bridge_connect_vae(bridge, vae), 0);
    ASSERT_EQ(vae_fep_bridge_connect_fep(bridge, fep), 0);

    // Encode input
    vae_forward(vae, input, recon, &latent);

    // Sync to FEP
    ASSERT_EQ(vae_fep_sync_latent_to_belief(bridge), 0);

    // Verify FEP beliefs match VAE latent
    fep_belief_state_t belief;
    fep_get_beliefs(fep, &belief);

    for (uint32_t i = 0; i < latent.latent_dim; i++) {
        ASSERT_NEAR(belief.mean[i], latent.mu[i], 1e-5);
    }

    cleanup(vae, fep, bridge);
}

// test_vae_immune_integration.cpp
TEST(VAEImmuneIntegration, AnomalyReporting) {
    vae_system_t* vae = create_test_vae();
    brain_immune_system_t* immune = create_test_immune();
    vae_immune_bridge_t* bridge = vae_immune_bridge_create(NULL);

    connect_all(vae, immune, bridge);

    // Force high reconstruction error
    nimcp_tensor_t* bad_input = create_adversarial_input();
    vae_forward(vae, bad_input, recon, &latent);

    // Check that anomaly was reported
    ASSERT_GT(bridge->stats.anomalies_reported, 0);

    // Verify immune system received antigen
    uint32_t antigen_count = brain_immune_get_antigen_count(immune);
    ASSERT_GT(antigen_count, 0);

    cleanup(vae, immune, bridge);
}

// test_vae_bio_async_integration.cpp
TEST(VAEBioAsyncIntegration, MessageRoundtrip) {
    vae_system_t* vae = create_test_vae();

    // Connect to bio-async router
    ASSERT_EQ(vae_connect_bio_async(vae), 0);

    // Send encode request message
    vae_encode_request_msg_t request = {
        .input = create_test_input()
    };

    nimcp_bio_promise_t promise;
    bio_router_send(BIO_MSG_VAE_ENCODE_REQUEST, &request, sizeof(request), &promise);

    // Wait for response
    vae_encode_response_msg_t response;
    ASSERT_EQ(bio_promise_wait(promise, &response, sizeof(response), 1000), 0);

    ASSERT_TRUE(response.success);
    ASSERT_EQ(response.latent_dim, vae->config.encoder.latent_dim);

    cleanup(vae);
}
```

#### Regression Tests

```cpp
// test_vae_numerical_stability.cpp
TEST(VAENumerical, GradientNaNDetection) {
    vae_system_t* vae = create_test_vae();

    // Create pathological input that might cause NaN
    nimcp_tensor_t* input = create_inf_input();

    vae_loss_t loss;
    int result = vae_train_step(vae, input, &loss);

    // Should fail gracefully and throw to immune
    ASSERT_EQ(result, -1);
    ASSERT_GT(get_immune_report_count_for_type(NIMCP_ERROR_VAE_GRADIENT_NAN), 0);

    cleanup(vae, input);
}

// test_vae_posterior_collapse.cpp
TEST(VAECollapse, PreventionMechanism) {
    vae_config_t config;
    vae_default_config(&config);
    config.training.free_bits = true;  // Enable free bits
    config.training.free_bits_lambda = 2.0f;

    vae_system_t* vae = vae_create(&config);

    // Train for many steps
    for (int i = 0; i < 10000; i++) {
        vae_train_step(vae, training_data[i], &loss);
    }

    // Check no dimensions collapsed
    vae_stats_t stats;
    vae_get_stats(vae, &stats);

    ASSERT_LT(stats.posterior_collapse_count, vae->config.encoder.latent_dim / 10);
    ASSERT_GT(stats.min_latent_variance, VAE_MIN_VARIANCE);

    cleanup(vae);
}

// test_vae_error_handling.cpp
TEST(VAEErrorHandling, NoSilentReturns) {
    // Test all API functions with invalid inputs
    // Verify each throws to immune system

    vae_system_t* vae = create_test_vae();

    // NULL input - should throw, not silent return
    int result = vae_encode(vae, NULL, NULL, NULL);
    ASSERT_EQ(result, -1);
    verify_immune_received_error(NIMCP_ERROR_VAE_NULL_POINTER);

    // Invalid dimensions
    nimcp_tensor_t* wrong_dim = nimcp_tensor_create_1d(999, NIMCP_DTYPE_FLOAT32);
    result = vae_encode(vae, wrong_dim, mu, log_var);
    ASSERT_EQ(result, -1);
    verify_immune_received_error(NIMCP_ERROR_VAE_INVALID_DIM);

    // Uninitialized VAE
    vae_system_t* uninit_vae = (vae_system_t*)nimcp_calloc(1, sizeof(vae_system_t));
    result = vae_forward(uninit_vae, input, recon, NULL);
    ASSERT_EQ(result, -1);
    verify_immune_received_error(NIMCP_ERROR_VAE_NOT_INITIALIZED);

    cleanup(vae, wrong_dim, uninit_vae);
}
```

#### End-to-End Tests

```cpp
// e2e_test_vae_fep_pipeline.cpp
TEST(E2EVAEFEP, FullPredictiveProcessingCycle) {
    // Create full system
    brain_t* brain = brain_create_test_instance();
    vae_system_t* vae = brain->vae;
    fep_system_t* fep = brain->fep;

    // 1. Receive observation
    nimcp_tensor_t* observation = get_sensory_input();

    // 2. VAE encodes to latent (recognition density)
    vae_latent_state_t latent;
    ASSERT_EQ(vae_forward(vae, observation, NULL, &latent), 0);

    // 3. Sync to FEP beliefs
    ASSERT_EQ(vae_fep_sync_latent_to_belief(brain->vae_fep_bridge), 0);

    // 4. FEP computes free energy
    fep_free_energy_t fe;
    ASSERT_EQ(fep_compute_free_energy(fep, &fe), 0);

    // 5. Generate prediction via VAE decoder
    nimcp_tensor_t* prediction;
    ASSERT_EQ(vae_decode(vae, latent.z, &prediction), 0);

    // 6. Compute prediction error
    float pred_error = compute_mse(observation, prediction);

    // 7. Update beliefs via VAE training
    vae_loss_t loss;
    ASSERT_EQ(vae_train_step(vae, observation, &loss), 0);

    // 8. Verify free energy decreased
    fep_free_energy_t new_fe;
    fep_compute_free_energy(fep, &new_fe);
    ASSERT_LE(new_fe.total, fe.total);

    cleanup(brain);
}

// e2e_test_vae_resilience.cpp
TEST(E2EVAEResilience, RecoveryFromFailure) {
    vae_system_t* vae = create_test_vae_with_immune();

    // 1. Normal operation
    for (int i = 0; i < 100; i++) {
        vae_train_step(vae, training_data[i], &loss);
    }

    vae_health_t health_before;
    vae_get_health(vae, &health_before);
    ASSERT_TRUE(health_before.is_healthy);

    // 2. Inject fault (bad data)
    nimcp_tensor_t* corrupted = create_corrupted_input();
    int result = vae_train_step(vae, corrupted, &loss);

    // 3. Verify error was caught and reported
    ASSERT_EQ(result, -1);

    vae_health_t health_during;
    vae_get_health(vae, &health_during);
    ASSERT_FALSE(health_during.is_healthy);
    ASSERT_GT(health_during.consecutive_errors, 0);

    // 4. System should recover with good data
    for (int i = 0; i < 10; i++) {
        result = vae_train_step(vae, training_data[i], &loss);
        if (result == 0) break;
    }

    ASSERT_EQ(result, 0);

    vae_health_t health_after;
    vae_get_health(vae, &health_after);
    ASSERT_TRUE(health_after.is_healthy);
    ASSERT_EQ(health_after.consecutive_errors, 0);

    cleanup(vae, corrupted);
}
```

---

## 11. File Manifest

### 11.1 Header Files (21 files)

| Path | Lines | Purpose |
|------|-------|---------|
| `include/cognitive/vae/nimcp_vae.h` | ~600 | Core VAE API |
| `include/cognitive/vae/nimcp_vae_encoder.h` | ~250 | Encoder network |
| `include/cognitive/vae/nimcp_vae_decoder.h` | ~250 | Decoder network |
| `include/cognitive/vae/nimcp_vae_latent.h` | ~200 | Latent operations |
| `include/cognitive/vae/nimcp_vae_loss.h` | ~150 | Loss computation |
| `include/cognitive/vae/nimcp_vae_config.h` | ~100 | Configuration types |
| `include/cognitive/vae/bridges/nimcp_vae_fep_bridge.h` | ~400 | FEP integration |
| `include/cognitive/vae/bridges/nimcp_vae_immune_bridge.h` | ~300 | Immune integration |
| `include/cognitive/vae/bridges/nimcp_vae_bbb_bridge.h` | ~200 | BBB integration |
| `include/cognitive/vae/bridges/nimcp_vae_hippocampus_bridge.h` | ~300 | Memory integration |
| `include/cognitive/vae/bridges/nimcp_vae_imagination_bridge.h` | ~250 | Imagination integration |
| `include/cognitive/vae/bridges/nimcp_vae_visual_bridge.h` | ~250 | Visual cortex |
| `include/cognitive/vae/bridges/nimcp_vae_auditory_bridge.h` | ~250 | Auditory cortex |
| `include/cognitive/vae/bridges/nimcp_vae_emotion_bridge.h` | ~200 | Emotion tensor |
| `include/cognitive/vae/bridges/nimcp_vae_introspection_bridge.h` | ~250 | Introspection |
| `include/cognitive/vae/bridges/nimcp_vae_world_model_bridge.h` | ~250 | World model |
| `include/cognitive/vae/bridges/nimcp_vae_snn_bridge.h` | ~300 | SNN encoding |
| `include/cognitive/vae/bridges/nimcp_vae_plasticity_bridge.h` | ~300 | STDP/plasticity |
| `include/cognitive/vae/bridges/nimcp_vae_training_bridge.h` | ~300 | Training layer |
| `include/cognitive/vae/bridges/nimcp_vae_substrate_bridge.h` | ~200 | Neural substrate |
| `include/cognitive/vae/bridges/nimcp_vae_thalamic_bridge.h` | ~200 | Thalamic relay |

### 11.2 Source Files (21 files)

| Path | Lines | Purpose |
|------|-------|---------|
| `src/cognitive/vae/nimcp_vae.c` | ~1200 | Core implementation |
| `src/cognitive/vae/nimcp_vae_encoder.c` | ~600 | Encoder implementation |
| `src/cognitive/vae/nimcp_vae_decoder.c` | ~600 | Decoder implementation |
| `src/cognitive/vae/nimcp_vae_latent.c` | ~400 | Latent operations |
| `src/cognitive/vae/nimcp_vae_loss.c` | ~350 | Loss computation |
| `src/cognitive/vae/bridges/nimcp_vae_fep_bridge.c` | ~800 | FEP bridge impl |
| `src/cognitive/vae/bridges/nimcp_vae_immune_bridge.c` | ~600 | Immune bridge impl |
| `src/cognitive/vae/bridges/nimcp_vae_bbb_bridge.c` | ~400 | BBB bridge impl |
| `src/cognitive/vae/bridges/nimcp_vae_hippocampus_bridge.c` | ~600 | Memory bridge impl |
| `src/cognitive/vae/bridges/nimcp_vae_imagination_bridge.c` | ~500 | Imagination impl |
| `src/cognitive/vae/bridges/nimcp_vae_visual_bridge.c` | ~500 | Visual bridge impl |
| `src/cognitive/vae/bridges/nimcp_vae_auditory_bridge.c` | ~500 | Auditory bridge impl |
| `src/cognitive/vae/bridges/nimcp_vae_emotion_bridge.c` | ~400 | Emotion bridge impl |
| `src/cognitive/vae/bridges/nimcp_vae_introspection_bridge.c` | ~500 | Introspection impl |
| `src/cognitive/vae/bridges/nimcp_vae_world_model_bridge.c` | ~500 | World model impl |
| `src/cognitive/vae/bridges/nimcp_vae_snn_bridge.c` | ~600 | SNN bridge impl |
| `src/cognitive/vae/bridges/nimcp_vae_plasticity_bridge.c` | ~600 | Plasticity bridge impl |
| `src/cognitive/vae/bridges/nimcp_vae_training_bridge.c` | ~600 | Training bridge impl |
| `src/cognitive/vae/bridges/nimcp_vae_substrate_bridge.c` | ~400 | Substrate bridge impl |
| `src/cognitive/vae/bridges/nimcp_vae_thalamic_bridge.c` | ~400 | Thalamic bridge impl |
| `src/cognitive/vae/bridges/nimcp_vae_logging_bridge.c` | ~300 | Logging bridge impl |

### 11.3 Test Files (36 files)

| Path | Tests | Purpose |
|------|-------|---------|
| Unit tests (8 files) | ~120 | Core functionality |
| Integration tests (15 files) | ~120 | Bridge integrations |
| Regression tests (7 files) | ~70 | Stability/safety |
| E2E tests (6 files) | ~30 | Full pipelines |

### 11.4 Summary

| Category | Files | Lines |
|----------|-------|-------|
| Headers | 21 | ~5,500 |
| Sources | 21 | ~10,850 |
| Tests | 36 | ~8,500 |
| Documentation | 1 | ~2,500 |
| **Total** | **79** | **~27,350** |

---

## 12. Implementation Phases

### Phase 1: Core VAE (Week 1-2)

| Task | Files | Priority |
|------|-------|----------|
| Core VAE header and config | `nimcp_vae.h`, `nimcp_vae_config.h` | P0 |
| Encoder implementation | `nimcp_vae_encoder.h/c` | P0 |
| Decoder implementation | `nimcp_vae_decoder.h/c` | P0 |
| Latent operations | `nimcp_vae_latent.h/c` | P0 |
| Loss computation | `nimcp_vae_loss.h/c` | P0 |
| Main VAE implementation | `nimcp_vae.c` | P0 |
| Unit tests | `test_vae_*.cpp` | P0 |

### Phase 2: FEP Integration (Week 3)

| Task | Files | Priority |
|------|-------|----------|
| VAE-FEP bridge header | `nimcp_vae_fep_bridge.h` | P0 |
| VAE-FEP bridge implementation | `nimcp_vae_fep_bridge.c` | P0 |
| Latent ↔ belief sync | Implementation | P0 |
| Free energy sharing | Implementation | P0 |
| Integration tests | `test_vae_fep_integration.cpp` | P0 |

### Phase 3: System Integrations (Week 4)

| Task | Files | Priority |
|------|-------|----------|
| Immune bridge | `nimcp_vae_immune_bridge.h/c` | P0 |
| BBB bridge | `nimcp_vae_bbb_bridge.h/c` | P0 |
| Health/heartbeat | All source files | P0 |
| KG wiring | All source files | P1 |
| Logging bridge | `nimcp_vae_logging_bridge.h/c` | P1 |
| Exception handling | All source files | P0 |

### Phase 4: Cognitive Bridges (Week 5-6)

| Task | Files | Priority |
|------|-------|----------|
| Hippocampus bridge | `nimcp_vae_hippocampus_bridge.h/c` | P1 |
| Imagination bridge | `nimcp_vae_imagination_bridge.h/c` | P1 |
| Visual bridge | `nimcp_vae_visual_bridge.h/c` | P1 |
| Auditory bridge | `nimcp_vae_auditory_bridge.h/c` | P2 |
| Emotion bridge | `nimcp_vae_emotion_bridge.h/c` | P1 |
| Introspection bridge | `nimcp_vae_introspection_bridge.h/c` | P1 |
| World model bridge | `nimcp_vae_world_model_bridge.h/c` | P1 |

### Phase 5: Neural Integrations (Week 7)

| Task | Files | Priority |
|------|-------|----------|
| SNN bridge | `nimcp_vae_snn_bridge.h/c` | P1 |
| Plasticity bridge | `nimcp_vae_plasticity_bridge.h/c` | P1 |
| Training bridge | `nimcp_vae_training_bridge.h/c` | P1 |
| Substrate bridge | `nimcp_vae_substrate_bridge.h/c` | P2 |
| Thalamic bridge | `nimcp_vae_thalamic_bridge.h/c` | P2 |

### Phase 6: Bio-Async & Testing (Week 8)

| Task | Files | Priority |
|------|-------|----------|
| Bio-async message types | Integration | P0 |
| Bio-async handlers | All bridges | P0 |
| Integration tests | All integration tests | P0 |
| Regression tests | All regression tests | P0 |
| E2E tests | All E2E tests | P0 |

---

## 13. Error Codes

| Code | Name | Description |
|------|------|-------------|
| 32400 | `NIMCP_ERROR_VAE_BASE` | Base VAE error |
| 32401 | `NIMCP_ERROR_VAE_NULL_POINTER` | NULL pointer passed |
| 32402 | `NIMCP_ERROR_VAE_INVALID_DIM` | Invalid dimension |
| 32403 | `NIMCP_ERROR_VAE_ENCODER_FAILED` | Encoder forward failed |
| 32404 | `NIMCP_ERROR_VAE_DECODER_FAILED` | Decoder forward failed |
| 32405 | `NIMCP_ERROR_VAE_LATENT_COLLAPSE` | Posterior collapse detected |
| 32406 | `NIMCP_ERROR_VAE_VARIANCE_EXPLODE` | Variance explosion |
| 32407 | `NIMCP_ERROR_VAE_KL_DIVERGE` | KL divergence abnormal |
| 32408 | `NIMCP_ERROR_VAE_RECON_FAILED` | Reconstruction failed |
| 32409 | `NIMCP_ERROR_VAE_GRADIENT_NAN` | NaN in gradients |
| 32410 | `NIMCP_ERROR_VAE_TRAINING_FAILED` | Training step failed |
| 32411 | `NIMCP_ERROR_VAE_NO_MEMORY` | Memory allocation failed |
| 32412 | `NIMCP_ERROR_VAE_INVALID_CONFIG` | Invalid configuration |
| 32413 | `NIMCP_ERROR_VAE_NOT_INITIALIZED` | VAE not initialized |
| 32414 | `NIMCP_ERROR_VAE_BRIDGE_FAILED` | Bridge operation failed |
| 32415 | `NIMCP_ERROR_VAE_BIO_ASYNC_FAILED` | Bio-async operation failed |

---

## Appendix A: CMake Integration

```cmake
# src/cognitive/vae/CMakeLists.txt

set(VAE_SOURCES
    nimcp_vae.c
    nimcp_vae_encoder.c
    nimcp_vae_decoder.c
    nimcp_vae_latent.c
    nimcp_vae_loss.c
    bridges/nimcp_vae_fep_bridge.c
    bridges/nimcp_vae_immune_bridge.c
    bridges/nimcp_vae_bbb_bridge.c
    bridges/nimcp_vae_hippocampus_bridge.c
    bridges/nimcp_vae_imagination_bridge.c
    bridges/nimcp_vae_visual_bridge.c
    bridges/nimcp_vae_auditory_bridge.c
    bridges/nimcp_vae_emotion_bridge.c
    bridges/nimcp_vae_introspection_bridge.c
    bridges/nimcp_vae_world_model_bridge.c
    bridges/nimcp_vae_snn_bridge.c
    bridges/nimcp_vae_plasticity_bridge.c
    bridges/nimcp_vae_training_bridge.c
    bridges/nimcp_vae_substrate_bridge.c
    bridges/nimcp_vae_thalamic_bridge.c
    bridges/nimcp_vae_logging_bridge.c
)

add_library(nimcp_vae STATIC ${VAE_SOURCES})

target_include_directories(nimcp_vae PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(nimcp_vae
    nimcp_tensor
    nimcp_free_energy
    nimcp_immune
    nimcp_bio_async
    nimcp_logging
    nimcp_thread
)
```

---

## Appendix B: KG Entity Definition

```jsonl
{"type": "entity", "name": "VAE_System", "entityType": "CognitiveModule", "observations": ["Variational Autoencoder for learned latent representations", "Integrates with FEP for belief/precision computation", "Provides generative sampling for imagination", "Detects anomalies via reconstruction error", "BIO_MODULE: 0x1F00"]}
{"type": "relation", "from": "VAE_System", "to": "FEP_System", "relationType": "provides_beliefs_to"}
{"type": "relation", "from": "VAE_System", "to": "Brain_Immune_System", "relationType": "reports_anomalies_to"}
{"type": "relation", "from": "VAE_System", "to": "Hippocampus", "relationType": "encodes_memories_for"}
{"type": "relation", "from": "VAE_System", "to": "Imagination_Engine", "relationType": "generates_samples_for"}
{"type": "relation", "from": "VAE_System", "to": "Introspection", "relationType": "provides_uncertainty_to"}
{"type": "relation", "from": "VAE_System", "to": "Bio_Async_Router", "relationType": "communicates_via"}
```

---

**End of Plan**

*This plan provides complete specifications for implementing a Variational Autoencoder module in NIMCP with full integration across 16+ cognitive and system modules.*
