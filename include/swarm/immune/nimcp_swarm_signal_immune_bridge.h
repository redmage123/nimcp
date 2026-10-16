/**
 * @file nimcp_swarm_signal_immune_bridge.h
 * @brief Swarm Signal-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Bidirectional integration between brain immune system and swarm signal transmission
 * WHY:  Inflammation affects signal quality; signal corruption triggers immune responses
 * HOW:  Cytokines increase packet loss; corrupted signals trigger antigen presentation
 *
 * BIOLOGICAL BASIS:
 * - Pro-inflammatory cytokines increase signal noise and packet loss
 * - Inflammation reduces transmission power and reception sensitivity
 * - Signal corruption triggers immune-like cleanup
 * - Clean signal paths release anti-inflammatory signals
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SWARM_SIGNAL_IMMUNE_BRIDGE_H
#define NIMCP_SWARM_SIGNAL_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/immune/nimcp_brain_immune.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Cytokine signal impact factors */
#define CYTOKINE_IL1_PACKET_LOSS              0.1f
#define CYTOKINE_IL6_PACKET_LOSS              0.08f
#define CYTOKINE_TNF_PACKET_LOSS              0.15f
#define CYTOKINE_IL10_SIGNAL_BOOST            0.1f

/* Inflammation signal factors */
#define INFLAMMATION_NONE_SIGNAL_FACTOR       1.0f
#define INFLAMMATION_LOCAL_SIGNAL_FACTOR      0.95f
#define INFLAMMATION_REGIONAL_SIGNAL_FACTOR   0.85f
#define INFLAMMATION_SYSTEMIC_SIGNAL_FACTOR   0.70f
#define INFLAMMATION_STORM_SIGNAL_FACTOR      0.40f

typedef struct {
    float packet_loss_increase;
    float signal_strength_reduction;
    float latency_increase;
    float noise_floor_increase;
} cytokine_signal_effects_t;

typedef struct {
    brain_inflammation_level_t current_level;
    float signal_quality_factor;
    float transmission_efficiency;
    bool critical_signals_only;
} inflammation_signal_state_t;

typedef struct {
    uint32_t corrupted_packets;
    float corruption_rate;
    bool immune_activated;
    float cleanup_signal;
} signal_immune_modulation_t;

typedef struct {
    bool enable_cytokine_effects;
    bool enable_inflammation_effects;
    bool enable_corruption_detection;
    float cytokine_sensitivity;
} swarm_signal_immune_config_t;

typedef struct swarm_signal_immune_bridge_struct swarm_signal_immune_bridge_t;

int swarm_signal_immune_default_config(swarm_signal_immune_config_t* config);
swarm_signal_immune_bridge_t* swarm_signal_immune_bridge_create(
    const swarm_signal_immune_config_t* config,
    brain_immune_system_t* immune_system,
    void* swarm_signal);
void swarm_signal_immune_bridge_destroy(swarm_signal_immune_bridge_t* bridge);

int swarm_signal_immune_apply_cytokine_effects(swarm_signal_immune_bridge_t* bridge);
int swarm_signal_immune_apply_inflammation_effects(swarm_signal_immune_bridge_t* bridge);

int swarm_signal_immune_report_corruption(swarm_signal_immune_bridge_t* bridge, uint32_t count);
int swarm_signal_immune_boost_from_clean_path(swarm_signal_immune_bridge_t* bridge);

int swarm_signal_immune_bridge_update(swarm_signal_immune_bridge_t* bridge, uint64_t delta_ms);

float swarm_signal_immune_get_quality_factor(const swarm_signal_immune_bridge_t* bridge);
float swarm_signal_immune_get_packet_loss(const swarm_signal_immune_bridge_t* bridge);
bool swarm_signal_immune_is_critical_only(const swarm_signal_immune_bridge_t* bridge);

int swarm_signal_immune_connect_bio_async(swarm_signal_immune_bridge_t* bridge);
int swarm_signal_immune_disconnect_bio_async(swarm_signal_immune_bridge_t* bridge);
bool swarm_signal_immune_is_bio_async_connected(const swarm_signal_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_SIGNAL_IMMUNE_BRIDGE_H */
