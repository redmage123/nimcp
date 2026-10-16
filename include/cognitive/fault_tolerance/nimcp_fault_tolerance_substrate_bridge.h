/**
 * @file nimcp_fault_tolerance_substrate_bridge.h
 * @brief Bridge between Fault Tolerance system and neural substrate
 *
 * WHAT: Links fault tolerance mechanisms to metabolic/energy state
 * WHY: Error detection and recovery require metabolic resources
 * HOW: Monitors ATP/fatigue; modulates detection sensitivity, recovery speed, redundancy
 *
 * BIOLOGICAL BASIS:
 * - Error detection involves prefrontal monitoring with high metabolic demand
 * - ATP depletion reduces error detection sensitivity
 * - Fatigue impairs recovery execution and increases error rates
 * - Metabolic stress affects redundancy maintenance
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_FAULT_TOLERANCE_SUBSTRATE_BRIDGE_H
#define NIMCP_FAULT_TOLERANCE_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_FAULT_TOLERANCE 0x1304

typedef struct {
    float detection_sensitivity;   /* Error detection sensitivity [0-1] */
    float recovery_speed;          /* Recovery execution speed [0-1] */
    float redundancy_capacity;     /* Redundancy maintenance capacity [0-1] */
    float monitoring_depth;        /* Monitoring coverage depth [0-1] */
    float overall_capacity;        /* Combined modulation [0-1] */
} fault_tolerance_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} fault_tolerance_substrate_config_t;

typedef struct fault_tolerance_substrate_bridge fault_tolerance_substrate_bridge_t;

fault_tolerance_substrate_config_t fault_tolerance_substrate_default_config(void);
fault_tolerance_substrate_bridge_t* fault_tolerance_substrate_bridge_create(void* fault_tolerance, neural_substrate_t* substrate, const fault_tolerance_substrate_config_t* config);
void fault_tolerance_substrate_bridge_destroy(fault_tolerance_substrate_bridge_t* bridge);
int fault_tolerance_substrate_bridge_update(fault_tolerance_substrate_bridge_t* bridge);
int fault_tolerance_substrate_bridge_get_effects(const fault_tolerance_substrate_bridge_t* bridge, fault_tolerance_substrate_effects_t* effects);
int fault_tolerance_substrate_bridge_apply_effects(fault_tolerance_substrate_bridge_t* bridge);
int fault_tolerance_substrate_bridge_register_bio_async(fault_tolerance_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FAULT_TOLERANCE_SUBSTRATE_BRIDGE_H */
