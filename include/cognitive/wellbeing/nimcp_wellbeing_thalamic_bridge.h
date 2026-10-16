/**
 * @file nimcp_wellbeing_thalamic_bridge.h
 * @brief Bridge between Wellbeing system and thalamic router
 *
 * WHAT: Routes wellbeing signals through thalamic attention pathways
 * WHY: Wellbeing states require attention for conscious awareness
 * HOW: Packages wellbeing signals, routes via insula-thalamic pathways
 *
 * BIOLOGICAL BASIS:
 * - Wellbeing involves insula-thalamic-ACC circuits
 * - MD nucleus integrates wellbeing with cognition
 * - VMPo nucleus relays interoceptive wellbeing signals
 * - Anterior thalamus links wellbeing to self-model
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_WELLBEING_THALAMIC_BRIDGE_H
#define NIMCP_WELLBEING_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WELLBEING_SIGNAL_STATUS       0x3701  /**< Wellbeing status */
#define WELLBEING_SIGNAL_CHANGE       0x3702  /**< State change */
#define WELLBEING_SIGNAL_THREAT       0x3703  /**< Wellbeing threat */
#define WELLBEING_SIGNAL_RECOVERY     0x3704  /**< Recovery signal */

typedef struct {
    uint32_t signal_type;
    float wellbeing_urgency;
    float current_level;
    float change_rate;
    float stability;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} wellbeing_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_threat_priority;
    float min_urgency_threshold;
    float threat_boost;
} wellbeing_thalamic_config_t;

typedef struct {
    uint64_t status_updates;
    uint64_t state_changes;
    uint64_t threats_signaled;
    uint64_t recoveries;
    uint64_t signals_gated;
    float avg_wellbeing_level;
    float avg_stability;
} wellbeing_thalamic_stats_t;

typedef struct wellbeing_thalamic_bridge wellbeing_thalamic_bridge_t;

wellbeing_thalamic_config_t wellbeing_thalamic_default_config(void);
wellbeing_thalamic_bridge_t* wellbeing_thalamic_bridge_create(
    void* wellbeing, thalamic_router_t* router,
    const wellbeing_thalamic_config_t* config);
void wellbeing_thalamic_bridge_destroy(wellbeing_thalamic_bridge_t* bridge);
int wellbeing_thalamic_bridge_reset(wellbeing_thalamic_bridge_t* bridge);

int wellbeing_thalamic_route_signal(
    wellbeing_thalamic_bridge_t* bridge,
    const wellbeing_thalamic_signal_t* signal);
int wellbeing_thalamic_route_status(
    wellbeing_thalamic_bridge_t* bridge,
    float level, float stability);
int wellbeing_thalamic_route_threat(
    wellbeing_thalamic_bridge_t* bridge,
    float severity, float urgency);

int wellbeing_thalamic_set_attention(wellbeing_thalamic_bridge_t* bridge, float attention);
int wellbeing_thalamic_get_attention(const wellbeing_thalamic_bridge_t* bridge, float* attention);
int wellbeing_thalamic_bridge_get_stats(
    const wellbeing_thalamic_bridge_t* bridge,
    wellbeing_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WELLBEING_THALAMIC_BRIDGE_H */
