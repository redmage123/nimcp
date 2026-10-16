/**
 * @file nimcp_hypothalamus_thalamic_bridge.h
 * @brief Bridge between Hypothalamus and thalamic router
 *
 * WHAT: Routes hypothalamic signals through thalamic relay
 * WHY: Hypothalamus connects with midline and anterior thalamic nuclei
 * HOW: Packages homeostatic signals, routes via appropriate pathway
 *
 * BIOLOGICAL BASIS:
 * - Anterior thalamus part of Papez circuit with hypothalamus
 * - Midline nuclei involved in arousal and homeostasis
 * - Homeostatic drives influence thalamic processing
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_HYPOTHALAMUS_THALAMIC_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HYPOTHALAMUS_SIGNAL_DRIVE       0x2C01
#define HYPOTHALAMUS_SIGNAL_HOMEOSTASIS 0x2C02
#define HYPOTHALAMUS_SIGNAL_CIRCADIAN   0x2C03
#define HYPOTHALAMUS_SIGNAL_STRESS      0x2C04

typedef struct {
    uint32_t signal_type;
    float drive_strength;
    float homeostatic_error;
    float circadian_phase;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} hypothalamus_thalamic_signal_t;

typedef struct {
    bool enable_drive_routing;
    bool enable_stress_priority;
    float min_drive_threshold;
    float stress_threshold;
} hypothalamus_thalamic_config_t;

typedef struct hypothalamus_thalamic_bridge hypothalamus_thalamic_bridge_t;

hypothalamus_thalamic_config_t hypothalamus_thalamic_default_config(void);
hypothalamus_thalamic_bridge_t* hypothalamus_thalamic_bridge_create(void* hypothalamus, thalamic_router_t* router, const hypothalamus_thalamic_config_t* config);
void hypothalamus_thalamic_bridge_destroy(hypothalamus_thalamic_bridge_t* bridge);
int hypothalamus_thalamic_bridge_reset(hypothalamus_thalamic_bridge_t* bridge);
int hypothalamus_thalamic_route_signal(hypothalamus_thalamic_bridge_t* bridge, const hypothalamus_thalamic_signal_t* signal);
int hypothalamus_thalamic_route_drive(hypothalamus_thalamic_bridge_t* bridge, uint32_t drive_type, float strength);
int hypothalamus_thalamic_set_attention(hypothalamus_thalamic_bridge_t* bridge, float attention);
int hypothalamus_thalamic_get_attention(const hypothalamus_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t drives_routed;
    uint64_t homeostatic_updates;
    uint64_t stress_responses;
    float avg_drive_strength;
} hypothalamus_thalamic_stats_t;

int hypothalamus_thalamic_bridge_get_stats(const hypothalamus_thalamic_bridge_t* bridge, hypothalamus_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_THALAMIC_BRIDGE_H */
