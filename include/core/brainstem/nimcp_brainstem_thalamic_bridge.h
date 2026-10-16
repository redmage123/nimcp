/**
 * @file nimcp_brainstem_thalamic_bridge.h
 * @brief Bridge between Brainstem and thalamic router
 *
 * WHAT: Routes brainstem signals through thalamic relay
 * WHY: Reticular formation and brainstem nuclei connect with intralaminar thalamus
 * HOW: Packages arousal/vital signals, routes via intralaminar pathway
 *
 * BIOLOGICAL BASIS:
 * - Intralaminar nuclei receive reticular activation
 * - Arousal system involves brainstem-thalamus connection
 * - Vital signals modulate thalamic gating
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_BRAINSTEM_THALAMIC_BRIDGE_H
#define NIMCP_BRAINSTEM_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BRAINSTEM_SIGNAL_AROUSAL     0x2B01
#define BRAINSTEM_SIGNAL_VITAL       0x2B02
#define BRAINSTEM_SIGNAL_REFLEX      0x2B03
#define BRAINSTEM_SIGNAL_AUTONOMIC   0x2B04

typedef struct {
    uint32_t signal_type;
    float arousal_level;
    float vital_urgency;
    float autonomic_state;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} brainstem_thalamic_signal_t;

typedef struct {
    bool enable_arousal_modulation;
    bool enable_vital_priority;
    float min_arousal_threshold;
    float vital_urgency_threshold;
} brainstem_thalamic_config_t;

typedef struct brainstem_thalamic_bridge brainstem_thalamic_bridge_t;

brainstem_thalamic_config_t brainstem_thalamic_default_config(void);
brainstem_thalamic_bridge_t* brainstem_thalamic_bridge_create(void* brainstem, thalamic_router_t* router, const brainstem_thalamic_config_t* config);
void brainstem_thalamic_bridge_destroy(brainstem_thalamic_bridge_t* bridge);
int brainstem_thalamic_bridge_reset(brainstem_thalamic_bridge_t* bridge);
int brainstem_thalamic_route_signal(brainstem_thalamic_bridge_t* bridge, const brainstem_thalamic_signal_t* signal);
int brainstem_thalamic_modulate_arousal(brainstem_thalamic_bridge_t* bridge, float level);
int brainstem_thalamic_set_attention(brainstem_thalamic_bridge_t* bridge, float attention);
int brainstem_thalamic_get_attention(const brainstem_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t arousal_updates;
    uint64_t vital_signals;
    uint64_t reflex_triggers;
    float avg_arousal_level;
} brainstem_thalamic_stats_t;

int brainstem_thalamic_bridge_get_stats(const brainstem_thalamic_bridge_t* bridge, brainstem_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAINSTEM_THALAMIC_BRIDGE_H */
