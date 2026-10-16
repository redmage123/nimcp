/**
 * @file nimcp_mental_health_thalamic_bridge.h
 * @brief Bridge between Mental Health system and thalamic router
 *
 * WHAT: Routes mental health signals through attention-gated thalamic pathways
 * WHY: Mental health monitoring requires conscious awareness via thalamic gating
 * HOW: Packages wellness signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Mental health involves whole-brain integration
 * - Thalamus coordinates emotional and cognitive processing
 * - Attention to wellbeing supports mental health
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_MENTAL_HEALTH_THALAMIC_BRIDGE_H
#define NIMCP_MENTAL_HEALTH_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MENTAL_HEALTH_SIGNAL_WELLBEING   0x1601
#define MENTAL_HEALTH_SIGNAL_STRESS      0x1602
#define MENTAL_HEALTH_SIGNAL_RESILIENCE  0x1603
#define MENTAL_HEALTH_SIGNAL_WARNING     0x1604

typedef struct {
    uint32_t signal_type;
    float wellbeing_level;
    float stress_level;
    float resilience_capacity;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} mental_health_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_warning_priority;
    float min_wellbeing_threshold;
    float stress_alert_threshold;
} mental_health_thalamic_config_t;

typedef struct mental_health_thalamic_bridge mental_health_thalamic_bridge_t;

mental_health_thalamic_config_t mental_health_thalamic_default_config(void);
mental_health_thalamic_bridge_t* mental_health_thalamic_bridge_create(void* mental_health, thalamic_router_t* router, const mental_health_thalamic_config_t* config);
void mental_health_thalamic_bridge_destroy(mental_health_thalamic_bridge_t* bridge);
int mental_health_thalamic_bridge_reset(mental_health_thalamic_bridge_t* bridge);
int mental_health_thalamic_route_wellbeing(mental_health_thalamic_bridge_t* bridge, const mental_health_thalamic_signal_t* signal);
int mental_health_thalamic_route_warning(mental_health_thalamic_bridge_t* bridge, const void* concern, float severity);
int mental_health_thalamic_set_attention(mental_health_thalamic_bridge_t* bridge, float attention);
int mental_health_thalamic_get_attention(const mental_health_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t wellbeing_updates;
    uint64_t stress_alerts;
    uint64_t warnings_issued;
    float avg_wellbeing_level;
} mental_health_thalamic_stats_t;

int mental_health_thalamic_bridge_get_stats(const mental_health_thalamic_bridge_t* bridge, mental_health_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MENTAL_HEALTH_THALAMIC_BRIDGE_H */
