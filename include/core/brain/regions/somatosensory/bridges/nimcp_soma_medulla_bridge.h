/**
 * @file nimcp_soma_medulla_bridge.h
 * @brief Somatosensory-Medulla Integration Bridge
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Bridge connecting somatosensory cortex to medulla oblongata for
 *       autonomic responses, protective reflexes, and visceral sensory
 *       integration.
 *
 * WHY: The medulla needs somatosensory input for:
 *      - Pain withdrawal reflexes (nociception)
 *      - Autonomic responses to touch/temperature
 *      - Blood pressure regulation (baroreceptors)
 *      - Respiratory adjustments
 *      - Gag/swallow reflexes
 *
 * HOW: Routes urgent pain signals for reflexive responses, temperature
 *      extremes for autonomic adjustment, and visceral sensory information.
 *
 * BIOLOGICAL BASIS:
 * =================
 * - Spinoreticular tracts carry pain to reticular formation
 * - Nucleus gracilis/cuneatus relay touch to medulla
 * - Medullary cardiovascular centers respond to somatosensory input
 * - Respiratory centers adjust to body position/movement
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SOMA_MEDULLA_BRIDGE_H
#define NIMCP_SOMA_MEDULLA_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "core/brain/regions/somatosensory/nimcp_somatosensory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SOMA_MEDULLA_PAIN_URGENT_THRESHOLD    0.8f
#define SOMA_MEDULLA_TEMP_DANGER_LOW          10.0f   /* Celsius */
#define SOMA_MEDULLA_TEMP_DANGER_HIGH         42.0f   /* Celsius */
#define SOMA_MEDULLA_REFLEX_LATENCY_MS        50

/* ============================================================================
 * Enumerations
 * ============================================================================ */

typedef enum {
    SOMA_MEDULLA_MSG_PAIN_URGENT = 0,    /**< Urgent pain (withdrawal) */
    SOMA_MEDULLA_MSG_TEMP_EXTREME,       /**< Extreme temperature */
    SOMA_MEDULLA_MSG_VISCERAL_INPUT,     /**< Visceral sensory input */
    SOMA_MEDULLA_MSG_AUTONOMIC_TRIGGER,  /**< Autonomic response trigger */
    SOMA_MEDULLA_MSG_REFLEX_REQUEST,     /**< Reflex request */
    SOMA_MEDULLA_MSG_RESPIRATORY_ADJ,    /**< Respiratory adjustment */
    SOMA_MEDULLA_MSG_COUNT
} soma_medulla_msg_type_t;

typedef enum {
    SOMA_MEDULLA_REFLEX_WITHDRAWAL = 0,  /**< Pain withdrawal */
    SOMA_MEDULLA_REFLEX_FLEXOR,          /**< Flexor reflex */
    SOMA_MEDULLA_REFLEX_CROSSED_EXT,     /**< Crossed extensor */
    SOMA_MEDULLA_REFLEX_GRASP,           /**< Grasp reflex */
    SOMA_MEDULLA_REFLEX_COUNT
} soma_medulla_reflex_t;

typedef enum {
    SOMA_MEDULLA_STATUS_IDLE = 0,
    SOMA_MEDULLA_STATUS_REFLEX_ACTIVE,
    SOMA_MEDULLA_STATUS_AUTONOMIC_ACTIVE,
    SOMA_MEDULLA_STATUS_ERROR
} soma_medulla_status_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Urgent pain signal
 */
typedef struct {
    body_segment_t region;        /**< Affected body region */
    float intensity;             /**< Pain intensity [0, 1] */
    bool is_sharp;               /**< Sharp (A-delta) vs dull (C-fiber) */
    bool requires_withdrawal;    /**< Requires immediate withdrawal */
    uint32_t source_id;          /**< Source receptor ID */
    uint64_t timestamp;          /**< Detection timestamp */
} soma_medulla_pain_t;

/**
 * @brief Temperature extreme signal
 */
typedef struct {
    body_segment_t region;        /**< Affected region */
    float temperature;           /**< Temperature (Celsius) */
    bool is_dangerous;           /**< Dangerous level */
    bool is_cold;                /**< Cold (vs hot) */
    uint64_t timestamp;          /**< Detection timestamp */
} soma_medulla_temp_extreme_t;

/**
 * @brief Reflex response
 */
typedef struct {
    soma_medulla_reflex_t type;  /**< Reflex type */
    body_segment_t affected;      /**< Affected region */
    float strength;              /**< Response strength [0, 1] */
    uint32_t latency_ms;         /**< Response latency */
    bool completed;              /**< Reflex completed */
} soma_medulla_reflex_response_t;

/**
 * @brief Autonomic adjustment request
 */
typedef struct {
    float heart_rate_delta;      /**< Heart rate change */
    float blood_pressure_delta;  /**< Blood pressure change */
    float respiration_delta;     /**< Respiration rate change */
    float perspiration;          /**< Perspiration level [0, 1] */
    uint64_t timestamp;          /**< Request timestamp */
} soma_medulla_autonomic_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

typedef struct {
    float pain_urgent_threshold;
    float temp_danger_low;
    float temp_danger_high;
    uint32_t reflex_latency_ms;
    bool enable_withdrawal_reflex;
    bool enable_autonomic_response;
    bool enable_logging;
} soma_medulla_config_t;

typedef struct {
    uint64_t pain_signals;
    uint64_t temp_extremes;
    uint64_t reflexes_triggered;
    uint64_t autonomic_adjustments;
    float avg_reflex_latency_ms;
} soma_medulla_stats_t;

/* ============================================================================
 * Handle
 * ============================================================================ */

typedef struct soma_medulla_bridge_struct soma_medulla_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int soma_medulla_default_config(soma_medulla_config_t* config);
soma_medulla_bridge_t* soma_medulla_bridge_create(const soma_medulla_config_t* config);
void soma_medulla_bridge_destroy(soma_medulla_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

int soma_medulla_connect(soma_medulla_bridge_t* bridge, nimcp_somatosensory_t* soma, void* medulla);
int soma_medulla_disconnect(soma_medulla_bridge_t* bridge);
bool soma_medulla_is_connected(const soma_medulla_bridge_t* bridge);

/* ============================================================================
 * Pain/Nociception API
 * ============================================================================ */

int soma_medulla_send_pain_urgent(soma_medulla_bridge_t* bridge, const soma_medulla_pain_t* pain);
int soma_medulla_request_withdrawal(soma_medulla_bridge_t* bridge, body_segment_t region);
int soma_medulla_check_reflex_status(soma_medulla_bridge_t* bridge, soma_medulla_reflex_response_t* response);

/* ============================================================================
 * Temperature API
 * ============================================================================ */

int soma_medulla_send_temp_extreme(soma_medulla_bridge_t* bridge, const soma_medulla_temp_extreme_t* temp);
int soma_medulla_request_thermoregulation(soma_medulla_bridge_t* bridge, float target_temp);

/* ============================================================================
 * Autonomic API
 * ============================================================================ */

int soma_medulla_trigger_autonomic(soma_medulla_bridge_t* bridge, const soma_medulla_autonomic_t* adj);
int soma_medulla_get_autonomic_state(soma_medulla_bridge_t* bridge, soma_medulla_autonomic_t* state);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int soma_medulla_get_stats(const soma_medulla_bridge_t* bridge, soma_medulla_stats_t* stats);
int soma_medulla_reset_stats(soma_medulla_bridge_t* bridge);
void soma_medulla_print_summary(const soma_medulla_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SOMA_MEDULLA_BRIDGE_H */
