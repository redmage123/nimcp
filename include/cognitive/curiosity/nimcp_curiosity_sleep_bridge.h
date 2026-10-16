/**
 * @file nimcp_curiosity_sleep_bridge.h
 * @brief Sleep-Curiosity Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and curiosity
 * WHY:  Exploration drive is fundamentally dependent on sleep state and rest
 * HOW:  Sleep state modulates curiosity intensity, exploration threshold, and learning potential
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Full curiosity drive, active exploration and learning
 * - DROWSY: Reduced curiosity (fatigue reduces novelty seeking)
 * - NREM: Curiosity offline (no active exploration during consolidation)
 * - REM: Internal exploration only (creative recombination of knowledge)
 *
 * Sleep deprivation causes:
 * - Reduced novelty seeking
 * - Impaired question generation
 * - Lower learning motivation
 * - Difficulty forming knowledge connections
 *
 * Sleep benefits curiosity:
 * - NREM consolidates learned knowledge gaps
 * - REM facilitates creative insight and distant associations
 * - Adequate sleep restores exploration drive
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CURIOSITY_SLEEP_BRIDGE_H
#define NIMCP_CURIOSITY_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */
#define CURIOSITY_SLEEP_DRIVE_AWAKE       1.0f
#define CURIOSITY_SLEEP_DRIVE_DROWSY      0.5f
#define CURIOSITY_SLEEP_DRIVE_LIGHT_NREM  0.0f
#define CURIOSITY_SLEEP_DRIVE_DEEP_NREM   0.0f
#define CURIOSITY_SLEEP_DRIVE_REM         0.4f  /* Internal exploration */

#define CURIOSITY_SLEEP_THRESHOLD_AWAKE      0.3f
#define CURIOSITY_SLEEP_THRESHOLD_DROWSY     0.6f  /* Higher threshold = less curious */
#define CURIOSITY_SLEEP_THRESHOLD_NREM       1.0f  /* No external exploration */
#define CURIOSITY_SLEEP_THRESHOLD_REM        0.5f

typedef struct {
    bool enable_drive_modulation;
    bool enable_threshold_modulation;
    float modulation_strength;
} curiosity_sleep_config_t;

typedef struct {
    float curiosity_drive_factor;
    float exploration_threshold_factor;
    float learning_potential_factor;
    sleep_state_t current_state;
    float sleep_pressure;
    bool exploration_offline;
} curiosity_sleep_effects_t;

typedef struct curiosity_sleep_bridge_struct* curiosity_sleep_bridge_t;

int curiosity_sleep_default_config(curiosity_sleep_config_t* config);
curiosity_sleep_bridge_t curiosity_sleep_bridge_create(const curiosity_sleep_config_t* config, sleep_system_t sleep);
void curiosity_sleep_bridge_destroy(curiosity_sleep_bridge_t bridge);
int curiosity_sleep_update(curiosity_sleep_bridge_t bridge);
int curiosity_sleep_get_effects(const curiosity_sleep_bridge_t bridge, curiosity_sleep_effects_t* effects);
float curiosity_sleep_get_drive(const curiosity_sleep_bridge_t bridge);
bool curiosity_sleep_is_offline(const curiosity_sleep_bridge_t bridge);

float curiosity_sleep_drive_for_state(sleep_state_t state);
float curiosity_sleep_threshold_for_state(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CURIOSITY_SLEEP_BRIDGE_H */
