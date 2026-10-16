/**
 * @file nimcp_motor_thalamic_bridge.h
 * @brief Bridge between Motor system and thalamic router
 *
 * WHAT: Routes motor signals through thalamic relay (VA/VL)
 * WHY: Motor commands pass through thalamus for cortical coordination
 * HOW: Packages motor signals, routes via ventral anterior/lateral pathway
 *
 * BIOLOGICAL BASIS:
 * - VA/VL nuclei relay basal ganglia and cerebellar output
 * - Motor thalamus coordinates cortical motor areas
 * - Motor planning and execution both involve thalamus
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_MOTOR_THALAMIC_BRIDGE_H
#define NIMCP_MOTOR_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOTOR_SIGNAL_PLAN        0x2301
#define MOTOR_SIGNAL_EXECUTE     0x2302
#define MOTOR_SIGNAL_FEEDBACK    0x2303
#define MOTOR_SIGNAL_INHIBIT     0x2304

typedef struct {
    uint32_t signal_type;
    float motor_urgency;
    float precision_requirement;
    uint32_t motor_program_id;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} motor_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_inhibition_routing;
    float min_urgency_threshold;
    float precision_threshold;
} motor_thalamic_config_t;

typedef struct motor_thalamic_bridge motor_thalamic_bridge_t;

motor_thalamic_config_t motor_thalamic_default_config(void);
motor_thalamic_bridge_t* motor_thalamic_bridge_create(void* motor, thalamic_router_t* router, const motor_thalamic_config_t* config);
void motor_thalamic_bridge_destroy(motor_thalamic_bridge_t* bridge);
int motor_thalamic_bridge_reset(motor_thalamic_bridge_t* bridge);
int motor_thalamic_route_signal(motor_thalamic_bridge_t* bridge, const motor_thalamic_signal_t* signal);
int motor_thalamic_route_inhibit(motor_thalamic_bridge_t* bridge, uint32_t program_id);
int motor_thalamic_set_attention(motor_thalamic_bridge_t* bridge, float attention);
int motor_thalamic_get_attention(const motor_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t plans_routed;
    uint64_t executions_relayed;
    uint64_t inhibitions_applied;
    float avg_motor_urgency;
} motor_thalamic_stats_t;

int motor_thalamic_bridge_get_stats(const motor_thalamic_bridge_t* bridge, motor_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MOTOR_THALAMIC_BRIDGE_H */
