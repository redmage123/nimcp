/**
 * @file nimcp_neuromodulatory_cognitive_bridge.h
 * @brief Unified Neuromodulatory - Cognitive Hub Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Bridge connecting all neuromodulatory centers (LC, VTA, Raphe, Habenula)
 *       to the cognitive integration hub, enabling bidirectional communication
 *       between neuromodulator states and cognitive processing.
 *
 * WHY: Neuromodulatory systems must coordinate with cognitive modules:
 *      - LC: Arousal and attention modulation
 *      - VTA: Reward and motivation signaling
 *      - Raphe: Mood and impulse control
 *      - Habenula: Aversive learning and avoidance
 *
 * HOW: Registers neuromodulatory modules with cognitive hub, subscribes to
 *      relevant cognitive events, and publishes neuromodulator state changes.
 *
 * BIOLOGICAL BASIS:
 * - NE (LC): Modulates attention, alertness, and gain control across cortex
 * - DA (VTA): Signals reward prediction errors for learning
 * - 5-HT (Raphe): Influences mood, patience, and social behavior
 * - Habenula: Inhibits DA/5-HT during aversive outcomes
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROMODULATORY_COGNITIVE_BRIDGE_H
#define NIMCP_NEUROMODULATORY_COGNITIVE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NEUROMOD_COG_MAX_EVENT_BUFFER       128
#define NEUROMOD_COG_DEFAULT_UPDATE_MS      50
#define NEUROMOD_COG_MAX_SUBSCRIPTIONS      32

/* Module IDs for cognitive hub registration */
#define NEUROMOD_COG_LC_MODULE_ID           0x4E454C43  /* "NELC" (NE Locus Coeruleus) */
#define NEUROMOD_COG_VTA_MODULE_ID          0x44415654  /* "DAVT" (DA VTA) */
#define NEUROMOD_COG_RAPHE_MODULE_ID        0x35485452  /* "5HTR" (5-HT Raphe) */
#define NEUROMOD_COG_HABENULA_MODULE_ID     0x48414245  /* "HABE" (Habenula) */

/* ============================================================================
 * Neuromodulatory Center Enumeration
 * ============================================================================ */

typedef enum {
    NEUROMOD_CENTER_LC = 0,             /**< Locus Coeruleus (NE) */
    NEUROMOD_CENTER_VTA,                /**< Ventral Tegmental Area (DA) */
    NEUROMOD_CENTER_RAPHE,              /**< Raphe Nuclei (5-HT) */
    NEUROMOD_CENTER_HABENULA,           /**< Habenula (aversive) */
    NEUROMOD_CENTER_COUNT
} neuromod_center_t;

/* ============================================================================
 * Cognitive Event Types
 * ============================================================================ */

typedef enum {
    /* LC events */
    NEUROMOD_COG_EVENT_AROUSAL_CHANGE = 0,
    NEUROMOD_COG_EVENT_GAIN_MODULATION,
    NEUROMOD_COG_EVENT_VIGILANCE_UPDATE,
    NEUROMOD_COG_EVENT_PHASIC_NE_BURST,

    /* VTA events */
    NEUROMOD_COG_EVENT_RPE_SIGNAL,
    NEUROMOD_COG_EVENT_MOTIVATION_UPDATE,
    NEUROMOD_COG_EVENT_VALUE_PREDICTION,
    NEUROMOD_COG_EVENT_DA_BURST,

    /* Raphe events */
    NEUROMOD_COG_EVENT_MOOD_CHANGE,
    NEUROMOD_COG_EVENT_IMPULSE_CONTROL,
    NEUROMOD_COG_EVENT_PATIENCE_UPDATE,
    NEUROMOD_COG_EVENT_SOCIAL_MODULATION,

    /* Habenula events */
    NEUROMOD_COG_EVENT_NEGATIVE_RPE,
    NEUROMOD_COG_EVENT_PUNISHMENT_SIGNAL,
    NEUROMOD_COG_EVENT_AVOIDANCE_TRIGGER,
    NEUROMOD_COG_EVENT_DISAPPOINTMENT,

    /* General */
    NEUROMOD_COG_EVENT_PLASTICITY_GATE,
    NEUROMOD_COG_EVENT_STATE_QUERY,
    NEUROMOD_COG_EVENT_COUNT
} neuromod_cog_event_t;

/* ============================================================================
 * State Structures
 * ============================================================================ */

/**
 * @brief Combined neuromodulatory state for cognitive hub
 */
typedef struct {
    /* LC state */
    float ne_level;
    float arousal;
    float alertness;
    float gain_modulation;
    bool phasic_mode;

    /* VTA state */
    float da_level;
    float motivation;
    float value_estimate;
    float last_rpe;
    bool reward_predicted;

    /* Raphe state */
    float ht_level;
    float mood;
    float impulse_control;
    float patience;
    float social_confidence;

    /* Habenula state */
    float habenula_activation;
    float negative_rpe;
    float avoidance_drive;
    float disappointment;
    bool punishment_detected;

    uint64_t timestamp_us;
} neuromod_cog_state_t;

/**
 * @brief Cognitive feedback state received from hub
 */
typedef struct {
    float cognitive_load;
    float attention_demand;
    float emotional_arousal;
    float learning_activity;
    float decision_confidence;
    uint64_t last_update_us;
} neuromod_cognitive_feedback_t;

/* ============================================================================
 * Event Payloads
 * ============================================================================ */

typedef struct {
    float arousal_level;
    float alertness;
    float gain_factor;
    bool phasic_burst;
    uint32_t trigger_source;
    uint64_t timestamp;
} neuromod_arousal_payload_t;

typedef struct {
    float rpe;
    float motivation;
    float value;
    bool positive_rpe;
    uint32_t context_id;
    uint64_t timestamp;
} neuromod_reward_payload_t;

typedef struct {
    float mood_level;
    float impulse_inhibition;
    float patience;
    float social_confidence;
    uint64_t timestamp;
} neuromod_mood_payload_t;

typedef struct {
    float negative_rpe;
    float avoidance_strength;
    float disappointment;
    uint32_t stimulus_id;
    bool urgent;
    uint64_t timestamp;
} neuromod_aversive_payload_t;

typedef struct {
    neuromod_center_t center;
    float gate_strength;
    float lr_multiplier;
    bool gate_open;
    uint32_t target_module;
    uint64_t timestamp;
} neuromod_plasticity_gate_payload_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

typedef struct {
    /* Enable flags per center */
    bool enable_lc_integration;
    bool enable_vta_integration;
    bool enable_raphe_integration;
    bool enable_habenula_integration;

    /* Broadcasting */
    float update_interval_ms;
    bool broadcast_on_change;

    /* Feedback */
    bool enable_cognitive_feedback;
    float cognitive_weight;

    /* Auto-subscribe */
    bool subscribe_emotion_updates;
    bool subscribe_attention_updates;
    bool subscribe_decision_updates;
    bool subscribe_learning_updates;

    /* Event buffer */
    uint32_t event_buffer_size;
} neuromod_cognitive_hub_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    /* Per-center stats */
    uint32_t lc_events_published;
    uint32_t vta_events_published;
    uint32_t raphe_events_published;
    uint32_t habenula_events_published;

    /* Feedback stats */
    uint32_t cognitive_updates_received;
    uint32_t attention_updates_received;
    uint32_t emotion_updates_received;

    /* Overall */
    uint32_t total_events_published;
    uint32_t total_events_received;
    uint32_t plasticity_gates_sent;

    /* Timing */
    uint64_t last_publish_time_us;
    uint64_t last_receive_time_us;
    float avg_latency_us;
} neuromod_cognitive_hub_stats_t;

/* ============================================================================
 * Bridge Handle
 * ============================================================================ */

typedef struct neuromod_cognitive_hub_bridge_struct neuromod_cognitive_hub_bridge_t;

/*
 * Forward declarations for adapters - only define if not already defined.
 * If including this header standalone, these provide minimal opaque types.
 * If including after adapter headers, those definitions take precedence.
 */
#ifndef NIMCP_LC_ADAPTER_H
typedef struct nimcp_lc_adapter_struct* nimcp_lc_adapter_t;
#endif

#ifndef NIMCP_VTA_ADAPTER_H
typedef struct nimcp_vta_adapter_struct* nimcp_vta_adapter_t;
#endif

#ifndef NIMCP_RAPHE_ADAPTER_H
typedef struct nimcp_raphe_adapter_struct* nimcp_raphe_adapter_t;
#endif

#ifndef NIMCP_HABENULA_ADAPTER_H
typedef struct nimcp_habenula_adapter_struct* nimcp_habenula_adapter_t;
#endif

/* Cognitive integration hub - always define as this header owns it */
typedef struct cognitive_integration_hub_struct* cognitive_integration_hub_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/* Lifecycle */
int neuromod_cognitive_hub_default_config(neuromod_cognitive_hub_config_t* config);
neuromod_cognitive_hub_bridge_t* neuromod_cognitive_hub_create(const neuromod_cognitive_hub_config_t* config);
void neuromod_cognitive_hub_destroy(neuromod_cognitive_hub_bridge_t* bridge);

/* Connection */
int neuromod_cognitive_hub_connect(
    neuromod_cognitive_hub_bridge_t* bridge,
    cognitive_integration_hub_t cog_hub
);
int neuromod_cognitive_hub_disconnect(neuromod_cognitive_hub_bridge_t* bridge);
bool neuromod_cognitive_hub_is_connected(const neuromod_cognitive_hub_bridge_t* bridge);

/* Adapter registration */
int neuromod_cognitive_hub_register_lc(neuromod_cognitive_hub_bridge_t* bridge, nimcp_lc_adapter_t adapter);
int neuromod_cognitive_hub_register_vta(neuromod_cognitive_hub_bridge_t* bridge, nimcp_vta_adapter_t adapter);
int neuromod_cognitive_hub_register_raphe(neuromod_cognitive_hub_bridge_t* bridge, nimcp_raphe_adapter_t adapter);
int neuromod_cognitive_hub_register_habenula(neuromod_cognitive_hub_bridge_t* bridge, nimcp_habenula_adapter_t adapter);

/* Update and processing */
int neuromod_cognitive_hub_update(neuromod_cognitive_hub_bridge_t* bridge, float delta_ms);
int neuromod_cognitive_hub_process_events(neuromod_cognitive_hub_bridge_t* bridge, uint32_t max_events);

/* Publishing */
int neuromod_cognitive_hub_publish_arousal(neuromod_cognitive_hub_bridge_t* bridge, const neuromod_arousal_payload_t* payload);
int neuromod_cognitive_hub_publish_reward(neuromod_cognitive_hub_bridge_t* bridge, const neuromod_reward_payload_t* payload);
int neuromod_cognitive_hub_publish_mood(neuromod_cognitive_hub_bridge_t* bridge, const neuromod_mood_payload_t* payload);
int neuromod_cognitive_hub_publish_aversive(neuromod_cognitive_hub_bridge_t* bridge, const neuromod_aversive_payload_t* payload);
int neuromod_cognitive_hub_publish_plasticity_gate(neuromod_cognitive_hub_bridge_t* bridge, const neuromod_plasticity_gate_payload_t* payload);

/* Broadcast all state */
int neuromod_cognitive_hub_broadcast_state(neuromod_cognitive_hub_bridge_t* bridge);

/* State access */
int neuromod_cognitive_hub_get_state(const neuromod_cognitive_hub_bridge_t* bridge, neuromod_cog_state_t* state);
int neuromod_cognitive_hub_get_feedback(const neuromod_cognitive_hub_bridge_t* bridge, neuromod_cognitive_feedback_t* feedback);

/* Statistics */
int neuromod_cognitive_hub_get_stats(const neuromod_cognitive_hub_bridge_t* bridge, neuromod_cognitive_hub_stats_t* stats);
int neuromod_cognitive_hub_reset_stats(neuromod_cognitive_hub_bridge_t* bridge);

/* Diagnostics */
const char* neuromod_center_name(neuromod_center_t center);
const char* neuromod_cog_event_name(neuromod_cog_event_t event);
void neuromod_cognitive_hub_print_summary(const neuromod_cognitive_hub_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROMODULATORY_COGNITIVE_BRIDGE_H */
