/**
 * @file nimcp_learning_constants.h
 * @brief Centralized learning and optimization constants for NIMCP
 * @version 1.0.0
 * @date 2026-02-15
 *
 * WHAT: Defines all learning-related constants used throughout the codebase
 * WHY:  Eliminates magic numbers, ensures consistency across modules
 * HOW:  Single header with hierarchical organization by learning domain
 *
 * Usage: #include "constants/nimcp_learning_constants.h"
 */

#ifndef NIMCP_LEARNING_CONSTANTS_H
#define NIMCP_LEARNING_CONSTANTS_H

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Learning Rate Constants
 *===========================================================================*/

/** @brief Default learning rate for general optimization (0.01) */
#define NIMCP_LEARNING_RATE_DEFAULT          0.01f

/** @brief Fine learning rate for precision updates (0.001) */
#define NIMCP_LEARNING_RATE_FINE             0.001f

/** @brief Coarse learning rate for fast convergence (0.1) */
#define NIMCP_LEARNING_RATE_COARSE           0.1f

/** @brief Micro learning rate for meta-learning / slow adaptation (0.0001) */
#define NIMCP_LEARNING_RATE_MICRO            0.0001f

/*=============================================================================
 * Momentum and Decay Constants
 *===========================================================================*/

/** @brief Default momentum for SGD-family optimizers (0.9) */
#define NIMCP_MOMENTUM_DEFAULT               0.9f

/** @brief High momentum for accelerated methods (0.99) */
#define NIMCP_MOMENTUM_HIGH                  0.99f

/** @brief Default EMA decay factor (0.99) */
#define NIMCP_EMA_DECAY_DEFAULT              0.99f

/** @brief Fast EMA decay factor (0.9) */
#define NIMCP_EMA_DECAY_FAST                 0.9f

/** @brief Slow EMA decay factor (0.999) */
#define NIMCP_EMA_DECAY_SLOW                 0.999f

/** @brief Default weight decay for regularization (0.0001) */
#define NIMCP_WEIGHT_DECAY_DEFAULT           0.0001f

/** @brief Default eligibility trace decay (0.95) */
#define NIMCP_ELIGIBILITY_DECAY_DEFAULT      0.95f

/*=============================================================================
 * Numerical Stability Constants
 *===========================================================================*/

/** @brief Default epsilon for numerical stability (1e-6) */
#define NIMCP_EPSILON_NUMERICAL              1e-6f

/** @brief Adam optimizer epsilon (1e-8) */
#define NIMCP_EPSILON_ADAM                    1e-8f

/** @brief Large epsilon for loose comparisons (1e-4) */
#define NIMCP_EPSILON_LARGE                  1e-4f

/** @brief Double-precision epsilon (1e-12) */
#define NIMCP_EPSILON_DOUBLE                 1e-12

/*=============================================================================
 * STDP (Spike-Timing-Dependent Plasticity) Constants
 *===========================================================================*/

/* Guarded — dendrite module (nimcp_dendrite.h) may define these first with
 * biologically-asymmetric values (17/34/0.005/0.00525). */

/** @brief STDP potentiation amplitude (A+) */
#ifndef NIMCP_STDP_A_PLUS
#define NIMCP_STDP_A_PLUS                    0.01f
#endif

/** @brief STDP depression amplitude (A-) */
#ifndef NIMCP_STDP_A_MINUS
#define NIMCP_STDP_A_MINUS                   0.012f
#endif

/** @brief STDP potentiation time constant in ms (tau+) */
#ifndef NIMCP_STDP_TAU_PLUS_MS
#define NIMCP_STDP_TAU_PLUS_MS               20.0f
#endif

/** @brief STDP depression time constant in ms (tau-) */
#ifndef NIMCP_STDP_TAU_MINUS_MS
#define NIMCP_STDP_TAU_MINUS_MS              20.0f
#endif

/*=============================================================================
 * Gradient and Convergence Constants
 *===========================================================================*/

/** @brief Default gradient clipping threshold (100.0 for 2M-neuron adjoint method) */
#define NIMCP_GRADIENT_CLIP_DEFAULT           100.0f

/** @brief Aggressive gradient clipping threshold */
#define NIMCP_GRADIENT_CLIP_TIGHT             0.5f

/** @brief Default maximum training iterations */
#define NIMCP_MAX_ITERATIONS_DEFAULT          1000

/** @brief Default convergence threshold */
#define NIMCP_CONVERGENCE_THRESHOLD           1e-6f

/** @brief Default batch size */
#define NIMCP_BATCH_SIZE_DEFAULT              32

/*=============================================================================
 * Adam / RMSProp Optimizer Constants
 *===========================================================================*/

/** @brief Adam beta1 (first moment decay) */
#define NIMCP_ADAM_BETA1_DEFAULT              0.9f

/** @brief Adam beta2 (second moment decay) */
#define NIMCP_ADAM_BETA2_DEFAULT              0.999f

/*=============================================================================
 * Reward and Reinforcement Constants
 *===========================================================================*/

/** @brief Default reward discount factor (gamma) */
#define NIMCP_REWARD_DISCOUNT_DEFAULT         0.99f

/** @brief Default reward scale */
#define NIMCP_REWARD_SCALE_DEFAULT            1.0f

/** @brief Default punishment scale */
#define NIMCP_PUNISHMENT_SCALE_DEFAULT        -0.5f

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LEARNING_CONSTANTS_H */
