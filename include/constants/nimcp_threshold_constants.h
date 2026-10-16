/**
 * @file nimcp_threshold_constants.h
 * @brief Centralized threshold and probability constants for NIMCP
 * @version 1.0.0
 * @date 2026-02-15
 *
 * WHAT: Defines all threshold, probability, and modulation constants
 * WHY:  Eliminates magic numbers, ensures consistent decision boundaries
 * HOW:  Single header with hierarchical organization by domain
 *
 * Usage: #include "constants/nimcp_threshold_constants.h"
 */

#ifndef NIMCP_THRESHOLD_CONSTANTS_H
#define NIMCP_THRESHOLD_CONSTANTS_H

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Probability Constants
 *===========================================================================*/

/** @brief Default probability threshold (0.5) */
#define NIMCP_PROBABILITY_DEFAULT             0.5f

/** @brief High probability threshold (0.8) */
#define NIMCP_PROBABILITY_HIGH                0.8f

/** @brief Low probability threshold (0.2) */
#define NIMCP_PROBABILITY_LOW                 0.2f

/** @brief Very high confidence threshold (0.9) */
#define NIMCP_PROBABILITY_VERY_HIGH           0.9f

/** @brief Very low probability threshold (0.1) */
#define NIMCP_PROBABILITY_VERY_LOW            0.1f

/*=============================================================================
 * Confidence Thresholds
 *===========================================================================*/

/** @brief High confidence threshold */
#define NIMCP_CONFIDENCE_HIGH                 0.8f

/** @brief Low confidence threshold */
#define NIMCP_CONFIDENCE_LOW                  0.3f

/** @brief Medium confidence threshold */
#define NIMCP_CONFIDENCE_MEDIUM               0.5f

/** @brief Minimum confidence for action selection */
#define NIMCP_CONFIDENCE_MIN                  0.1f

/*=============================================================================
 * Activation and Novelty Thresholds
 *===========================================================================*/

/** @brief Default activation threshold */
#define NIMCP_ACTIVATION_THRESHOLD            0.5f

/** @brief Default novelty detection threshold */
#define NIMCP_NOVELTY_THRESHOLD               0.7f

/** @brief Default salience threshold */
#define NIMCP_SALIENCE_THRESHOLD              0.3f

/** @brief Default attention threshold */
#define NIMCP_ATTENTION_THRESHOLD             0.5f

/** @brief Default conflict detection threshold */
#define NIMCP_CONFLICT_THRESHOLD              0.5f

/*=============================================================================
 * Modulation Range Constants
 *===========================================================================*/

/** @brief Minimum modulation value (clamping floor) */
#define NIMCP_MODULATION_MIN                  0.0f

/** @brief Maximum modulation value (clamping ceiling) */
#define NIMCP_MODULATION_MAX                  1.0f

/** @brief Default modulation level */
#define NIMCP_MODULATION_DEFAULT              0.5f

/** @brief Minimum sensitivity value */
#define NIMCP_SENSITIVITY_MIN                 0.0f

/** @brief Maximum sensitivity value */
#define NIMCP_SENSITIVITY_MAX                 2.0f

/** @brief Default sensitivity value */
#define NIMCP_SENSITIVITY_DEFAULT             1.0f

/*=============================================================================
 * Temperature and Exploration Constants
 *===========================================================================*/

/** @brief Default softmax temperature */
#define NIMCP_TEMPERATURE_DEFAULT             1.0f

/** @brief High temperature for more exploration */
#define NIMCP_TEMPERATURE_HIGH                2.0f

/** @brief Low temperature for more exploitation */
#define NIMCP_TEMPERATURE_LOW                 0.5f

/** @brief Default exploration rate (epsilon-greedy) */
#define NIMCP_EXPLORATION_RATE_DEFAULT        0.1f

/*=============================================================================
 * Similarity and Distance Thresholds
 *===========================================================================*/

/** @brief Default similarity threshold for matching */
#define NIMCP_SIMILARITY_THRESHOLD            0.8f

/** @brief Default distance threshold */
#define NIMCP_DISTANCE_THRESHOLD              0.5f

/** @brief Cosine similarity threshold for "close enough" */
#define NIMCP_COSINE_SIMILARITY_HIGH          0.9f

/*=============================================================================
 * Signal Range Clamping
 *===========================================================================*/

/** @brief Minimum signal value (bipolar: -1 to +1) */
#define NIMCP_SIGNAL_MIN_BIPOLAR              -1.0f

/** @brief Maximum signal value (bipolar: -1 to +1) */
#define NIMCP_SIGNAL_MAX_BIPOLAR              1.0f

/** @brief Minimum signal value (unipolar: 0 to +1) */
#define NIMCP_SIGNAL_MIN_UNIPOLAR             0.0f

/** @brief Maximum signal value (unipolar: 0 to +1) */
#define NIMCP_SIGNAL_MAX_UNIPOLAR             1.0f

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_THRESHOLD_CONSTANTS_H */
