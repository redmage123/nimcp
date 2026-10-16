/**
 * @file nimcp_sequence_detector_fep_bridge.h
 * @brief Free Energy Principle - Sequence Detector Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and sequence detection
 * WHY:  Sequences represent temporal structure in FEP generative models; sequence
 *       violations generate prediction errors; FEP predictions facilitate detection
 * HOW:  FEP predictions → expected sequences; detected sequences → temporal observations;
 *       sequence violations → prediction errors
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * FEP → SEQUENCE DETECTION PATHWAYS:
 * -----------------------------------
 * 1. Predictions as Expected Sequences:
 *    - FEP temporal predictions = expected sequence templates
 *    - Facilitates sequence detection (predictive priming)
 *    - Reduced detection thresholds for expected sequences
 *    - Reference: Bubic et al. (2010) "Prediction, cognition and the brain"
 *
 * 2. Precision Modulates Detection Thresholds:
 *    - High precision → strict sequence matching (low tolerance)
 *    - Low precision → loose matching (high tolerance)
 *    - Adaptive sequence detection based on confidence
 *    - Reference: Feldman & Friston (2010) "Attention, uncertainty, and free-energy"
 *
 * 3. Hierarchical Sequence Representation:
 *    - FEP hierarchy levels → sequence granularity
 *    - Low levels → short sequences (n-grams)
 *    - High levels → long sequences (episodes)
 *    - Reference: Friston (2008) "Hierarchical models in the brain"
 *
 * SEQUENCE DETECTION → FEP PATHWAYS:
 * -----------------------------------
 * 1. Detected Sequences as Temporal Observations:
 *    - Sequence matches confirm predictions
 *    - Sequence strength → observation confidence
 *    - Temporal structure observation for FEP
 *    - Reference: Friston & Kiebel (2009) "Predictive coding under the free-energy
 *      principle"
 *
 * 2. Sequence Violations Generate Prediction Errors:
 *    - Unexpected sequences → high PE
 *    - Missing expected sequences → PE
 *    - Triggers belief updating and learning
 *    - Reference: Rao & Ballard (1999) "Predictive coding in the visual cortex"
 *
 * 3. Replay Detection as Memory Consolidation:
 *    - Forward/backward replay → offline FEP updating
 *    - Compressed replay → efficient memory consolidation
 *    - Sleep/rest state sequence processing
 *    - Reference: Foster & Wilson (2006) "Reverse replay of behavioural sequences
 *      in hippocampal place cells"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SEQUENCE_DETECTOR_FEP_BRIDGE_H
#define NIMCP_SEQUENCE_DETECTOR_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "middleware/patterns/nimcp_sequence_detector.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */
#define FEP_PRECISION_HIGH_TOLERANCE     0.8f   /**< Strict matching */
#define FEP_PRECISION_LOW_TOLERANCE      0.3f   /**< Loose matching */
#define FEP_SEQUENCE_VIOLATION_PE        8.0f   /**< PE for violations */
#define FEP_REPLAY_CONSOLIDATION_BONUS   -1.0f  /**< Free energy bonus for replay */

typedef struct sequence_detector_fep_bridge sequence_detector_fep_bridge_t;

typedef struct {
    bool enable_prediction_priming;
    bool enable_precision_tolerance;
    bool enable_sequence_pe;
    bool enable_replay_consolidation;
    float tolerance_sensitivity;
    float pe_sensitivity;
} sequence_detector_fep_config_t;

typedef struct {
    float detection_threshold;
    float temporal_tolerance;
    uint32_t expected_template_id;
    bool priming_active;
} sequence_detector_fep_effects_t;

typedef struct {
    float current_precision;
    uint32_t sequences_detected;
    uint32_t violations_detected;
    float replay_consolidation_progress;
} sequence_detector_fep_state_t;

typedef struct {
    uint64_t sequence_detections;
    uint64_t sequence_violations;
    uint64_t replay_events;
    float avg_sequence_strength;
    float avg_pe;
} sequence_detector_fep_stats_t;

struct sequence_detector_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    sequence_detector_fep_config_t config;
    sequence_detector_t* sequence_detector;
    fep_system_t* fep_system;
    sequence_detector_fep_effects_t effects;
    sequence_detector_fep_state_t state;
    sequence_detector_fep_stats_t stats;
};

/* Lifecycle */
int sequence_detector_fep_bridge_default_config(sequence_detector_fep_config_t* config);
sequence_detector_fep_bridge_t* sequence_detector_fep_bridge_create(const sequence_detector_fep_config_t* config);
void sequence_detector_fep_bridge_destroy(sequence_detector_fep_bridge_t* bridge);

/* Connection */
int sequence_detector_fep_bridge_connect_detector(sequence_detector_fep_bridge_t* bridge, sequence_detector_t* detector);
int sequence_detector_fep_bridge_connect_fep(sequence_detector_fep_bridge_t* bridge, fep_system_t* fep);
int sequence_detector_fep_bridge_disconnect(sequence_detector_fep_bridge_t* bridge);

/* FEP → Sequence Detector */
int sequence_detector_fep_prime_expected_sequence(sequence_detector_fep_bridge_t* bridge, uint32_t template_id);
int sequence_detector_fep_adjust_tolerance(sequence_detector_fep_bridge_t* bridge, float precision);

/* Sequence Detector → FEP */
int sequence_detector_fep_report_detection(sequence_detector_fep_bridge_t* bridge, const sequence_detection_t* detection);
int sequence_detector_fep_report_violation(sequence_detector_fep_bridge_t* bridge, uint32_t expected_id);
int sequence_detector_fep_report_replay(sequence_detector_fep_bridge_t* bridge, const sequence_detection_t* replay);

/* Update */
int sequence_detector_fep_bridge_update(sequence_detector_fep_bridge_t* bridge, uint64_t delta_ms);

/* State/Stats */
int sequence_detector_fep_bridge_get_state(const sequence_detector_fep_bridge_t* bridge, sequence_detector_fep_state_t* state);
int sequence_detector_fep_bridge_get_stats(const sequence_detector_fep_bridge_t* bridge, sequence_detector_fep_stats_t* stats);

/* Bio-Async */
int sequence_detector_fep_bridge_connect_bio_async(sequence_detector_fep_bridge_t* bridge);
int sequence_detector_fep_bridge_disconnect_bio_async(sequence_detector_fep_bridge_t* bridge);
bool sequence_detector_fep_bridge_is_bio_async_connected(const sequence_detector_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SEQUENCE_DETECTOR_FEP_BRIDGE_H */
