/**
 * @file nimcp_fault_tolerance_thalamic_bridge.h
 * @brief Bridge between Fault Tolerance system and thalamic router
 *
 * WHAT: Routes fault detection signals through attention-gated thalamic pathways
 * WHY: Critical errors require immediate conscious attention via thalamic routing
 * HOW: Packages error signals, routes via thalamic attention mechanism with priority
 *
 * BIOLOGICAL BASIS:
 * - Error detection involves anterior cingulate cortex with thalamic loops
 * - Critical errors get enhanced thalamic routing priority
 * - Attention is rapidly redirected to high-severity faults
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_FAULT_TOLERANCE_THALAMIC_BRIDGE_H
#define NIMCP_FAULT_TOLERANCE_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FAULT_SIGNAL_DETECT     0x0601
#define FAULT_SIGNAL_RECOVER    0x0602
#define FAULT_SIGNAL_ESCALATE   0x0603
#define FAULT_SIGNAL_RESOLVE    0x0604

typedef struct {
    uint32_t signal_type;
    float severity;
    float criticality;
    float urgency;
    void* fault_data;
    uint32_t data_size;
    uint64_t timestamp_us;
} fault_tolerance_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_severity_boost;
    float min_severity_threshold;
    float escalation_boost;
} fault_tolerance_thalamic_config_t;

typedef struct fault_tolerance_thalamic_bridge fault_tolerance_thalamic_bridge_t;

fault_tolerance_thalamic_config_t fault_tolerance_thalamic_default_config(void);
fault_tolerance_thalamic_bridge_t* fault_tolerance_thalamic_bridge_create(void* fault_tolerance, thalamic_router_t* router, const fault_tolerance_thalamic_config_t* config);
void fault_tolerance_thalamic_bridge_destroy(fault_tolerance_thalamic_bridge_t* bridge);
int fault_tolerance_thalamic_bridge_reset(fault_tolerance_thalamic_bridge_t* bridge);
int fault_tolerance_thalamic_route_detection(fault_tolerance_thalamic_bridge_t* bridge, const fault_tolerance_thalamic_signal_t* signal);
int fault_tolerance_thalamic_route_recovery(fault_tolerance_thalamic_bridge_t* bridge, const void* recovery_plan, float priority);
int fault_tolerance_thalamic_set_attention(fault_tolerance_thalamic_bridge_t* bridge, float attention);
int fault_tolerance_thalamic_get_attention(fault_tolerance_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t detections_routed;
    uint64_t recoveries_initiated;
    uint64_t escalations_triggered;
    float avg_severity;
} fault_tolerance_thalamic_stats_t;

int fault_tolerance_thalamic_bridge_get_stats(fault_tolerance_thalamic_bridge_t* bridge, fault_tolerance_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FAULT_TOLERANCE_THALAMIC_BRIDGE_H */
