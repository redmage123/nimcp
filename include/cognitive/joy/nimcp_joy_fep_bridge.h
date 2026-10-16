/**
 * @file nimcp_joy_fep_bridge.h
 * @brief Free Energy Principle - Joy/Euphoria Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between FEP and joy/euphoria system
 * WHY:  Joy arises from positive prediction errors (reward prediction errors).
 *       Better-than-expected outcomes generate positive affect.
 * HOW:  FEP positive PEs trigger joy; joy modulates dopamine-mediated learning.
 *
 * BIOLOGICAL BASIS:
 * - Reward prediction error = Positive PE in FEP framework
 * - Positive RPE → Dopamine release → Joy/euphoria
 * - Joy enhances learning rate for value-aligned successes
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_JOY_FEP_BRIDGE_H
#define NIMCP_JOY_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_joy_euphoria.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float positive_pe_joy_gain;
    float euphoria_threshold;
    bool enable_positive_pe_joy;
    bool enable_euphoria_detection;
    float joy_learning_rate_boost;
    float joy_dopamine_modulation;
    bool enable_joy_learning_boost;
    bool enable_joy_dopamine;
    float fe_sensitivity;
    float emotion_sensitivity;
} joy_fep_config_t;

typedef struct {
    float joy_intensity_from_pe;
    float positive_valence;
    bool euphoria_triggered;
} joy_fep_effects_t;

typedef struct {
    float learning_rate_boost;
    float dopamine_multiplier;
    float value_update_gain;
} fep_joy_effects_t;

typedef struct {
    float current_positive_pe;
    float joy_intensity;
    float euphoria_intensity;
    bool joyful;
    bool euphoric;
} joy_fep_state_t;

typedef struct {
    uint64_t joy_events;
    uint64_t euphoria_events;
    float avg_joy_intensity;
    float avg_positive_pe;
} joy_fep_stats_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    joy_fep_config_t config;
    fep_system_t* fep_system;
    joy_system_t* joy_system;
    joy_fep_effects_t fep_effects;
    fep_joy_effects_t emotion_effects;
    joy_fep_state_t state;
    joy_fep_stats_t stats;} joy_fep_bridge_t;

int joy_fep_default_config(joy_fep_config_t* config);
joy_fep_bridge_t* joy_fep_create(const joy_fep_config_t* config);
void joy_fep_destroy(joy_fep_bridge_t* bridge);

int joy_fep_connect_fep(joy_fep_bridge_t* bridge, fep_system_t* fep);
int joy_fep_connect_joy(joy_fep_bridge_t* bridge, joy_system_t* joy);
int joy_fep_disconnect(joy_fep_bridge_t* bridge);

int joy_fep_process_positive_pe(joy_fep_bridge_t* bridge, float pe_magnitude);
int joy_fep_boost_learning_rate(joy_fep_bridge_t* bridge);
int joy_fep_update(joy_fep_bridge_t* bridge, uint64_t delta_ms);

int joy_fep_get_state(joy_fep_bridge_t* bridge, joy_fep_state_t* state);
int joy_fep_get_stats(joy_fep_bridge_t* bridge, joy_fep_stats_t* stats);

int joy_fep_connect_bio_async(joy_fep_bridge_t* bridge);
int joy_fep_disconnect_bio_async(joy_fep_bridge_t* bridge);
bool joy_fep_is_bio_async_connected(const joy_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_JOY_FEP_BRIDGE_H */
