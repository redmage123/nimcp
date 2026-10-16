/**
 * @file nimcp_neuromodulatory_immune_bridge.h
 * @brief Unified Neuromodulatory-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Bidirectional bridge connecting all neuromodulatory centers (LC, VTA, Raphe, Habenula)
 *       to the brain immune system (microglia, cytokines, inflammation response).
 *
 * WHY: Neuromodulatory systems profoundly influence immune function (psychoneuroimmunology):
 *      - LC (NE): Stress-induced immunomodulation, NE receptors on immune cells
 *      - VTA (DA): Reward/motivation affects immune resilience
 *      - Raphe (5-HT): Mood modulates inflammatory responses
 *      - Habenula: Chronic stress/depression immunosuppression
 *
 * HOW: Neuromodulator levels modulate cytokine production, inflammation thresholds,
 *      and immune cell activity. Immune signals (cytokines) feed back to affect
 *      neuromodulator release and neural function ("sickness behavior").
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * PSYCHONEUROIMMUNOLOGY MECHANISMS:
 *
 * 1. Norepinephrine (LC) and Immunity:
 *    - Sympathetic innervation of lymphoid organs
 *    - NE receptors (beta-adrenergic) on T cells, B cells, macrophages
 *    - Acute stress: Immunostimulatory (enhanced surveillance)
 *    - Chronic stress: Immunosuppressive (glucocorticoid synergy)
 *    - Reference: Elenkov et al., "Stress Hormones and Immunity"
 *
 * 2. Dopamine (VTA) and Immunity:
 *    - DA receptors on T cells and dendritic cells
 *    - Reward/positive affect: Anti-inflammatory, immune resilience
 *    - Anhedonia/low DA: Pro-inflammatory cytokine increase
 *    - Reference: Sarkar et al., "Dopamine and Immune Function"
 *
 * 3. Serotonin (Raphe) and Immunity:
 *    - Peripheral 5-HT: 95% in gut, immune modulator
 *    - 5-HT receptors on immune cells
 *    - Anti-inflammatory at moderate levels (via 5-HT2A)
 *    - Depression (low 5-HT) associates with inflammation
 *    - Reference: Baganz & Bhagwagar, "Serotonin and Immune Interaction"
 *
 * 4. Habenula and Chronic Stress/Inflammation:
 *    - Hyperactive habenula = chronic stress/depression
 *    - Suppresses DA/5-HT, promoting inflammation
 *    - Links negative affect to immune dysfunction
 *    - Reference: Hikosaka, "Habenula and Aversive Motivation"
 *
 * NEUROMODULATOR -> IMMUNE EFFECTS:
 * ---------------------------------
 * High NE (acute): IL-6 boost (fight-or-flight immune mobilization)
 * High NE (chronic): IL-10 suppression (immunosuppression)
 * High DA: IL-10 increase, TNF-alpha decrease (anti-inflammatory)
 * Low DA: TNF-alpha increase (pro-inflammatory)
 * High 5-HT: IL-10 increase (anti-inflammatory, tolerance)
 * Low 5-HT: IL-1, IL-6 increase (depression-inflammation)
 * High Habenula: Cortisol proxy, general immunosuppression
 *
 * IMMUNE -> NEUROMODULATOR EFFECTS (Sickness Behavior):
 * -----------------------------------------------------
 * IL-1beta: Decreases NE turnover, causes fatigue/anhedonia
 * IL-6: Modulates DA release, affects motivation
 * TNF-alpha: Reduces 5-HT synthesis, depressive symptoms
 * IFN-gamma: Activates IDO, depletes tryptophan -> low 5-HT
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |          NEUROMODULATORY-IMMUNE UNIFIED BRIDGE                            |
 * +===========================================================================+
 * |                                                                           |
 * |   NEUROMODULATORY                     IMMUNE SYSTEM                       |
 * |   +-------------------+               +-------------------+               |
 * |   | LC (NE)           |<--IL-1------->| Cytokine         |               |
 * |   | Acute/Chronic     |<--Fatigue---->| Production       |               |
 * |   +-------------------+               +-------------------+               |
 * |   +-------------------+               +-------------------+               |
 * |   | VTA (DA)          |<--IL-6------->| Inflammation     |               |
 * |   | Reward/Motivation |<--Anhedonia-->| Level            |               |
 * |   +-------------------+               +-------------------+               |
 * |   +-------------------+               +-------------------+               |
 * |   | Raphe (5-HT)      |<--TNF/IFN---->| Microglial       |               |
 * |   | Mood/Tolerance    |<--IDO-------->| Activation       |               |
 * |   +-------------------+               +-------------------+               |
 * |   +-------------------+               +-------------------+               |
 * |   | Habenula          |<--Cortisol--->| Immunosuppression|               |
 * |   | Chronic Stress    |<--HPA axis--->| or Autoimmunity  |               |
 * |   +-------------------+               +-------------------+               |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROMODULATORY_IMMUNE_BRIDGE_H
#define NIMCP_NEUROMODULATORY_IMMUNE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NEUROMOD_IMM_MAX_EVENT_BUFFER       128
#define NEUROMOD_IMM_DEFAULT_UPDATE_MS      100
#define NEUROMOD_IMM_MAX_SUBSCRIPTIONS      32

/* Magic number for validation */
#define NEUROMOD_IMMUNE_BRIDGE_MAGIC        0x4E494D42  /* "NIMB" */

/* Neuromodulator -> Immune effect magnitudes (from PNI literature) */
#define NE_ACUTE_IL6_BOOST                  0.20f   /* Acute stress immune mobilization */
#define NE_CHRONIC_IL10_SUPPRESS            0.15f   /* Chronic stress immunosuppression */
#define DA_IL10_BOOST                       0.15f   /* Reward anti-inflammatory */
#define DA_TNF_SUPPRESS                     0.12f   /* DA suppresses TNF-alpha */
#define HT_IL10_BOOST                       0.18f   /* Serotonin anti-inflammatory */
#define HT_LOW_INFLAMMATION_BOOST           0.20f   /* Low 5-HT pro-inflammatory */
#define HAB_IMMUNOSUPPRESS_FACTOR           0.25f   /* Chronic stress immunosuppression */

/* Immune -> Neuromodulator effect magnitudes (sickness behavior) */
#define IL1_NE_FATIGUE_FACTOR               0.15f   /* IL-1 causes fatigue */
#define IL6_DA_ANHEDONIA_FACTOR             0.12f   /* IL-6 affects motivation */
#define TNF_5HT_SUPPRESS_FACTOR             0.20f   /* TNF reduces 5-HT */
#define IFN_TRYPTOPHAN_DEPLETION            0.25f   /* IFN-gamma depletes tryptophan */

/* ============================================================================
 * Neuromodulatory Immune Event Types
 * ============================================================================ */

typedef enum {
    /* Neuromodulatory -> Immune events */
    NEUROMOD_IMM_EVENT_NE_ACUTE_STRESS = 0,     /**< Acute stress immune mobilization */
    NEUROMOD_IMM_EVENT_NE_CHRONIC_STRESS,        /**< Chronic stress immunosuppression */
    NEUROMOD_IMM_EVENT_DA_REWARD_STATE,          /**< Reward/positive affect */
    NEUROMOD_IMM_EVENT_DA_ANHEDONIA,             /**< Low motivation state */
    NEUROMOD_IMM_EVENT_5HT_MOOD_POSITIVE,        /**< Good mood, anti-inflammatory */
    NEUROMOD_IMM_EVENT_5HT_MOOD_NEGATIVE,        /**< Depression, pro-inflammatory */
    NEUROMOD_IMM_EVENT_HAB_CHRONIC_AVERSION,     /**< Chronic stress/aversion */

    /* Immune -> Neuromodulatory events (sickness behavior) */
    NEUROMOD_IMM_EVENT_IL1_FATIGUE,              /**< Cytokine-induced fatigue */
    NEUROMOD_IMM_EVENT_IL6_MOTIVATIONAL,         /**< Affects motivation */
    NEUROMOD_IMM_EVENT_TNF_DEPRESSIVE,           /**< Depressive-like effects */
    NEUROMOD_IMM_EVENT_IFN_TRYPTOPHAN,           /**< Tryptophan depletion */
    NEUROMOD_IMM_EVENT_INFLAMMATION_GENERAL,     /**< General inflammation signal */

    NEUROMOD_IMM_EVENT_COUNT
} neuromod_imm_event_t;

/* ============================================================================
 * Immune Modulation State
 * ============================================================================ */

/**
 * @brief How neuromodulators affect immune parameters
 */
typedef struct {
    /* From LC (NE) effects */
    float ne_level;                     /**< Current NE level [0-1] */
    float acute_stress_mobilization;    /**< Immune mobilization from acute stress */
    float chronic_stress_suppression;   /**< Immunosuppression from chronic stress */
    bool acute_stress_mode;             /**< In acute stress response */
    bool chronic_stress_mode;           /**< In chronic stress state */

    /* From VTA (DA) effects */
    float da_level;                     /**< Current DA level [0-1] */
    float reward_anti_inflammatory;     /**< Anti-inflammatory from positive state */
    float anhedonia_pro_inflammatory;   /**< Pro-inflammatory from anhedonia */
    bool positive_affect;               /**< Currently in positive state */

    /* From Raphe (5-HT) effects */
    float ht_level;                     /**< Current 5-HT level [0-1] */
    float mood_anti_inflammatory;       /**< Anti-inflammatory from good mood */
    float depression_pro_inflammatory;  /**< Pro-inflammatory from depression */
    bool good_mood;                     /**< Currently in good mood */

    /* From Habenula effects */
    float habenula_activation;          /**< Habenula activity level [0-1] */
    float chronic_aversion_suppression; /**< Immunosuppression from chronic aversion */

    /* Net cytokine modulation (output to immune system) */
    float il1_modulation;               /**< IL-1beta production modulation */
    float il6_modulation;               /**< IL-6 production modulation */
    float il10_modulation;              /**< IL-10 (anti-inflammatory) modulation */
    float tnf_modulation;               /**< TNF-alpha production modulation */
    float ifn_modulation;               /**< IFN-gamma modulation */

    uint64_t timestamp_us;
} neuromod_immune_modulation_t;

/**
 * @brief Immune feedback to neuromodulatory systems (sickness behavior)
 */
typedef struct {
    /* Cytokine levels (input from immune system) */
    float il1_level;                    /**< IL-1beta level [0-1] */
    float il6_level;                    /**< IL-6 level [0-1] */
    float il10_level;                   /**< IL-10 level [0-1] */
    float tnf_level;                    /**< TNF-alpha level [0-1] */
    float ifn_level;                    /**< IFN-gamma level [0-1] */

    /* Inflammation state */
    float inflammation_level;           /**< Overall inflammation [0-1] */
    bool systemic_inflammation;         /**< Systemic inflammatory state */
    bool cytokine_storm;                /**< Critical inflammation */

    /* Sickness behavior effects (output to neuromodulators) */
    float fatigue_induction;            /**< Fatigue signal to LC */
    float anhedonia_induction;          /**< Anhedonia signal to VTA */
    float depression_induction;         /**< Depressive signal to Raphe */
    float tryptophan_depletion;         /**< 5-HT precursor depletion */

    uint64_t last_update_us;
} neuromod_immune_feedback_t;

/* ============================================================================
 * Event Payloads
 * ============================================================================ */

typedef struct {
    float ne_level;
    float stress_duration_ms;           /**< Duration affects acute vs chronic */
    float mobilization_strength;
    bool phasic_response;
    uint64_t timestamp;
} neuromod_imm_ne_payload_t;

typedef struct {
    float da_level;
    float rpe;                          /**< Reward prediction error */
    float motivation;
    bool positive_outcome;
    uint64_t timestamp;
} neuromod_imm_da_payload_t;

typedef struct {
    float ht_level;
    float mood_valence;                 /**< -1 (depressed) to +1 (positive) */
    float social_context;
    uint64_t timestamp;
} neuromod_imm_ht_payload_t;

typedef struct {
    float habenula_activation;
    float aversion_duration_ms;
    float suppression_strength;
    uint64_t timestamp;
} neuromod_imm_hab_payload_t;

typedef struct {
    float il1_level;
    float il6_level;
    float il10_level;
    float tnf_level;
    float ifn_level;
    float inflammation_level;
    bool urgent;
    uint64_t timestamp;
} neuromod_imm_cytokine_payload_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

typedef struct {
    /* Enable flags per center */
    bool enable_lc_immune_modulation;
    bool enable_vta_immune_modulation;
    bool enable_raphe_immune_modulation;
    bool enable_habenula_immune_modulation;

    /* Enable immune feedback (sickness behavior) */
    bool enable_cytokine_feedback;
    bool enable_inflammation_feedback;

    /* Modulation strengths */
    float ne_acute_weight;              /**< Acute stress mobilization weight */
    float ne_chronic_weight;            /**< Chronic stress suppression weight */
    float da_reward_weight;             /**< Reward anti-inflammatory weight */
    float ht_mood_weight;               /**< Mood modulation weight */
    float hab_suppression_weight;       /**< Habenula suppression weight */

    /* Sickness behavior sensitivity */
    float il1_fatigue_sensitivity;
    float il6_anhedonia_sensitivity;
    float tnf_depression_sensitivity;
    float ifn_depletion_sensitivity;

    /* Chronic stress threshold (ms of continuous stress) */
    float chronic_stress_threshold_ms;

    /* Timing */
    float update_interval_ms;
    bool broadcast_on_change;

    /* Event buffer */
    uint32_t event_buffer_size;
} neuromod_immune_bridge_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    /* Outbound effects per center */
    uint32_t lc_immune_modulations;
    uint32_t vta_immune_modulations;
    uint32_t raphe_immune_modulations;
    uint32_t habenula_immune_modulations;

    /* Inbound cytokine feedback */
    uint32_t cytokine_events_received;
    uint32_t inflammation_events_received;

    /* Sickness behavior triggers */
    uint32_t fatigue_signals_sent;
    uint32_t anhedonia_signals_sent;
    uint32_t depression_signals_sent;

    /* Correlation tracking */
    uint32_t high_stress_inflammation_correlation;
    uint32_t positive_affect_recovery_correlation;

    /* Overall */
    uint32_t total_events_sent;
    uint32_t total_events_received;
    uint64_t last_activity_us;
} neuromod_immune_bridge_stats_t;

/* ============================================================================
 * Bridge Handle
 * ============================================================================ */

typedef struct neuromod_immune_bridge_struct neuromod_immune_bridge_t;

/* Forward declarations for adapters */
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

/* Immune system handle (opaque) */
typedef struct nimcp_immune_system_struct* nimcp_immune_system_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/* Lifecycle */
int neuromod_immune_bridge_default_config(neuromod_immune_bridge_config_t* config);
neuromod_immune_bridge_t* neuromod_immune_bridge_create(const neuromod_immune_bridge_config_t* config);
void neuromod_immune_bridge_destroy(neuromod_immune_bridge_t* bridge);

/* Connection */
int neuromod_immune_bridge_connect_immune(neuromod_immune_bridge_t* bridge, nimcp_immune_system_t immune);
int neuromod_immune_bridge_disconnect(neuromod_immune_bridge_t* bridge);
bool neuromod_immune_bridge_is_connected(const neuromod_immune_bridge_t* bridge);

/* Adapter registration */
int neuromod_immune_bridge_register_lc(neuromod_immune_bridge_t* bridge, nimcp_lc_adapter_t adapter);
int neuromod_immune_bridge_register_vta(neuromod_immune_bridge_t* bridge, nimcp_vta_adapter_t adapter);
int neuromod_immune_bridge_register_raphe(neuromod_immune_bridge_t* bridge, nimcp_raphe_adapter_t adapter);
int neuromod_immune_bridge_register_habenula(neuromod_immune_bridge_t* bridge, nimcp_habenula_adapter_t adapter);

/* Update and processing */
int neuromod_immune_bridge_update(neuromod_immune_bridge_t* bridge, float delta_ms);
int neuromod_immune_bridge_process_events(neuromod_immune_bridge_t* bridge, uint32_t max_events);

/* Neuromodulatory -> Immune modulation */
int neuromod_immune_apply_ne_stress(neuromod_immune_bridge_t* bridge, const neuromod_imm_ne_payload_t* payload);
int neuromod_immune_apply_da_reward(neuromod_immune_bridge_t* bridge, const neuromod_imm_da_payload_t* payload);
int neuromod_immune_apply_ht_mood(neuromod_immune_bridge_t* bridge, const neuromod_imm_ht_payload_t* payload);
int neuromod_immune_apply_hab_aversion(neuromod_immune_bridge_t* bridge, const neuromod_imm_hab_payload_t* payload);

/* Immune -> Neuromodulatory feedback (sickness behavior) */
int neuromod_immune_report_cytokines(neuromod_immune_bridge_t* bridge, const neuromod_imm_cytokine_payload_t* payload);

/* Compute net cytokine modulation from current state */
int neuromod_immune_compute_modulation(neuromod_immune_bridge_t* bridge);

/* State access */
int neuromod_immune_bridge_get_modulation(const neuromod_immune_bridge_t* bridge, neuromod_immune_modulation_t* modulation);
int neuromod_immune_bridge_get_feedback(const neuromod_immune_bridge_t* bridge, neuromod_immune_feedback_t* feedback);

/* Statistics */
int neuromod_immune_bridge_get_stats(const neuromod_immune_bridge_t* bridge, neuromod_immune_bridge_stats_t* stats);
int neuromod_immune_bridge_reset_stats(neuromod_immune_bridge_t* bridge);

/* Diagnostics */
const char* neuromod_imm_event_name(neuromod_imm_event_t event);
void neuromod_immune_bridge_print_summary(const neuromod_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROMODULATORY_IMMUNE_BRIDGE_H */
