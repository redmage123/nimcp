/**
 * @file nimcp_auditory_thalamic_bridge.h
 * @brief Bridge between Auditory processing and thalamic router
 *
 * WHAT: Routes auditory signals through thalamic relay (MGN)
 * WHY: All auditory information passes through MGN to cortex
 * HOW: Packages auditory signals, routes via medial geniculate nucleus pathway
 *
 * BIOLOGICAL BASIS:
 * - MGN (medial geniculate nucleus) is primary auditory relay
 * - Tonotopic organization preserved through relay
 * - TRN modulates auditory attention
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_AUDITORY_THALAMIC_BRIDGE_H
#define NIMCP_AUDITORY_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUDITORY_SIGNAL_PRIMARY    0x2101
#define AUDITORY_SIGNAL_SPEECH     0x2102
#define AUDITORY_SIGNAL_MUSIC      0x2103
#define AUDITORY_SIGNAL_ALERT      0x2104

typedef struct {
    uint32_t signal_type;
    float auditory_salience;
    float attention_weight;
    float frequency_center;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} auditory_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_speech_priority;
    float min_salience_threshold;
    float speech_boost;
} auditory_thalamic_config_t;

typedef struct auditory_thalamic_bridge auditory_thalamic_bridge_t;

auditory_thalamic_config_t auditory_thalamic_default_config(void);
auditory_thalamic_bridge_t* auditory_thalamic_bridge_create(void* auditory, thalamic_router_t* router, const auditory_thalamic_config_t* config);
void auditory_thalamic_bridge_destroy(auditory_thalamic_bridge_t* bridge);
int auditory_thalamic_bridge_reset(auditory_thalamic_bridge_t* bridge);
int auditory_thalamic_route_signal(auditory_thalamic_bridge_t* bridge, const auditory_thalamic_signal_t* signal);
int auditory_thalamic_route_alert(auditory_thalamic_bridge_t* bridge, const void* alert, float urgency);
int auditory_thalamic_set_attention(auditory_thalamic_bridge_t* bridge, float attention);
int auditory_thalamic_get_attention(const auditory_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t signals_relayed;
    uint64_t speech_processed;
    uint64_t alerts_triggered;
    float avg_auditory_salience;
} auditory_thalamic_stats_t;

int auditory_thalamic_bridge_get_stats(const auditory_thalamic_bridge_t* bridge, auditory_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUDITORY_THALAMIC_BRIDGE_H */
