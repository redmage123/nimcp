/**
 * @file nimcp_language_hypothalamus_bridge.c
 * @brief Hypothalamus ↔ grounded language for drive states and motivation
 *
 * WHAT: Integrates homeostatic drives and motivational states with language
 * WHY:  Enables embodied language grounded in biological needs and goals
 * HOW:  Hypothalamic state (hunger, thirst, arousal, circadian) → drive semantics
 *
 * Hypothalamic language functions:
 *   - Drive expression: "I am hungry", "I need water", "I am tired"
 *   - Motivational language: goal-oriented utterances from drive states
 *   - Emotional coloring: affect-laden language from limbic coupling
 *   - Circadian modulation: time-of-day effects on language production
 *   - Homeostatic urgency: drive intensity modulates semantic salience
 *
 * Biological basis:
 *   - Hunger/satiety: lateral vs ventromedial hypothalamic nuclei
 *   - Thirst/osmotic balance: supraoptic nucleus
 *   - Sleep/wake: suprachiasmatic nucleus (SCN) circadian pacemaker
 *   - Stress response: paraventricular nucleus (PVN) → HPA axis
 *   - Thermoregulation: preoptic area
 *
 * @author NIMCP Development Team
 * @date 2026-10-21
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_internal.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_handler.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <string.h>
#include <time.h>

/**
 * @brief Homeostatic drive states from hypothalamus
 */
typedef struct {
    float hunger;           /**< Hunger level [0=sated, 1=starving] */
    float thirst;           /**< Thirst level [0=hydrated, 1=dehydrated] */
    float fatigue;          /**< Fatigue/sleep pressure [0=alert, 1=exhausted] */
    float stress;           /**< Stress level [0=calm, 1=stressed] */
    float temperature;      /**< Thermal state [-1=cold, 0=comfortable, +1=hot] */
    float arousal;          /**< General arousal [0=low, 1=high] */
    float circadian_phase;  /**< Circadian rhythm phase [0=midnight, 0.5=noon, 1=midnight] */
} hypothalamic_drive_state_t;

/**
 * @brief Motivational state derived from drives
 */
typedef struct {
    char dominant_drive[64];  /**< Name of strongest current drive */
    float drive_intensity;    /**< Intensity of dominant drive [0,1] */
    float goal_urgency;       /**< How urgent current goals are */
    float affective_valence;  /**< Emotional valence from drive satisfaction */
} motivational_state_t;

/**
 * @brief Extract homeostatic drive states from brain
 *
 * @param brain       Brain instance
 * @param out_drives  Output drive state (allocated by caller)
 * @return 0 on success, -1 on error
 */
int hypothalamus_extract_drives(brain_t brain,
                                  hypothalamic_drive_state_t* out_drives) {
    if (!brain || !out_drives) return -1;

    /* Query brainstem adapter which maintains metabolic/homeostatic state.
     * The brainstem update (called in brain_learn_vector Round A) tracks
     * energy balance, hydration, sleep pressure, etc. */

    /* Placeholder: derive from brain activity patterns and time.
     * Actual implementation: query brain->brainstem->homeostatic_state. */

    /* Simulate circadian rhythm based on time of day */
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    float hour_fraction = (tm_info->tm_hour + tm_info->tm_min / 60.0f) / 24.0f;
    out_drives->circadian_phase = hour_fraction;

    /* Sleep pressure increases with time awake (simplified) */
    out_drives->fatigue = 0.3f + 0.4f * sinf(2.0f * M_PI * hour_fraction);
    out_drives->fatigue = fmaxf(0.0f, fminf(1.0f, out_drives->fatigue));

    /* Energy/hunger from metabolic state (placeholder) */
    out_drives->hunger = 0.2f + 0.3f * ((float)rand() / RAND_MAX);

    /* Hydration/thirst (placeholder) */
    out_drives->thirst = 0.1f + 0.2f * ((float)rand() / RAND_MAX);

    /* Stress from cognitive load (approximate from brain activity) */
    out_drives->stress = 0.15f;

    /* Temperature (assume comfortable) */
    out_drives->temperature = 0.0f;

    /* General arousal (correlates with activity) */
    out_drives->arousal = 0.5f + 0.3f * (1.0f - out_drives->fatigue);

    NIMCP_LOGGING_DEBUG("Hypothalamus drives: hunger=%.2f thirst=%.2f fatigue=%.2f "
                        "stress=%.2f circadian=%.2f arousal=%.2f",
                        out_drives->hunger, out_drives->thirst, out_drives->fatigue,
                        out_drives->stress, out_drives->circadian_phase,
                        out_drives->arousal);

    return 0;
}

/**
 * @brief Derive motivational state from drive states
 *
 * Identifies dominant drive and computes motivational intensity.
 *
 * @param drives     Homeostatic drive states
 * @param out_motiv  Output motivational state (allocated by caller)
 * @return 0 on success, -1 on error
 */
int hypothalamus_derive_motivation(const hypothalamic_drive_state_t* drives,
                                     motivational_state_t* out_motiv) {
    if (!drives || !out_motiv) return -1;

    /* Find dominant drive (highest intensity) */
    float max_drive = drives->hunger;
    strncpy(out_motiv->dominant_drive, "hunger", sizeof(out_motiv->dominant_drive) - 1);

    if (drives->thirst > max_drive) {
        max_drive = drives->thirst;
        strncpy(out_motiv->dominant_drive, "thirst", sizeof(out_motiv->dominant_drive) - 1);
    }
    if (drives->fatigue > max_drive) {
        max_drive = drives->fatigue;
        strncpy(out_motiv->dominant_drive, "fatigue", sizeof(out_motiv->dominant_drive) - 1);
    }
    if (drives->stress > max_drive) {
        max_drive = drives->stress;
        strncpy(out_motiv->dominant_drive, "stress", sizeof(out_motiv->dominant_drive) - 1);
    }

    out_motiv->drive_intensity = max_drive;
    out_motiv->goal_urgency = max_drive; /* Urgency correlates with drive intensity */

    /* Affective valence: low when drives are unsatisfied, high when satisfied */
    float drive_dissatisfaction = (drives->hunger + drives->thirst +
                                    drives->fatigue + drives->stress) / 4.0f;
    out_motiv->affective_valence = 1.0f - drive_dissatisfaction;

    return 0;
}

/**
 * @brief Convert drive state to motivational language
 *
 * Generates language expressing current needs and goals.
 *
 * @param drives      Drive states
 * @param motivation  Motivational state
 * @param out_text    Output motivational utterance (caller allocates buffer)
 * @param buffer_size Size of output buffer
 * @return 0 on success, -1 on error
 */
int hypothalamus_drives_to_language(const hypothalamic_drive_state_t* drives,
                                      const motivational_state_t* motivation,
                                      char* out_text,
                                      size_t buffer_size) {
    if (!drives || !motivation || !out_text) return -1;

    /* Generate motivational language based on dominant drive */
    if (strcmp(motivation->dominant_drive, "hunger") == 0) {
        if (drives->hunger > 0.7f) {
            strncpy(out_text, "I am very hungry and need food", buffer_size - 1);
        } else if (drives->hunger > 0.4f) {
            strncpy(out_text, "I feel hungry", buffer_size - 1);
        } else {
            strncpy(out_text, "I have some appetite", buffer_size - 1);
        }
    } else if (strcmp(motivation->dominant_drive, "thirst") == 0) {
        if (drives->thirst > 0.7f) {
            strncpy(out_text, "I am very thirsty and need water", buffer_size - 1);
        } else {
            strncpy(out_text, "I feel thirsty", buffer_size - 1);
        }
    } else if (strcmp(motivation->dominant_drive, "fatigue") == 0) {
        if (drives->fatigue > 0.7f) {
            strncpy(out_text, "I am exhausted and need rest", buffer_size - 1);
        } else {
            strncpy(out_text, "I feel tired", buffer_size - 1);
        }
    } else if (strcmp(motivation->dominant_drive, "stress") == 0) {
        strncpy(out_text, "I feel stressed", buffer_size - 1);
    } else {
        strncpy(out_text, "I feel comfortable", buffer_size - 1);
    }
    out_text[buffer_size - 1] = '\0';

    return 0;
}

/**
 * @brief Integrate hypothalamic drives into grounded language
 *
 * Modulates language production based on homeostatic and motivational state.
 *
 * @param brain  Brain with hypothalamus/brainstem
 * @param gl     Grounded language instance
 * @param state  Cascade state with content_intent
 * @return 0 on success, -1 on error
 */
int grounded_language_integrate_hypothalamus(brain_t brain,
                                               grounded_language_t* gl,
                                               void* cascade_state) {
    if (!brain || !gl || !cascade_state) return -1;

    /* Check if hypothalamus coupling is enabled */
    extern bool grounded_language_hypothalamus_enabled(const grounded_language_t* gl);
    if (!grounded_language_hypothalamus_enabled(gl)) {
        return 0; /* Coupling disabled */
    }

    /* Get cascade state */
    typedef struct {
        float* content_intent;
        uint32_t dim;
    } cascade_state_t;
    cascade_state_t* cs = (cascade_state_t*)cascade_state;

    /* Extract homeostatic drives */
    hypothalamic_drive_state_t drives;
    if (hypothalamus_extract_drives(brain, &drives) != 0) {
        return -1;
    }

    /* Derive motivational state */
    motivational_state_t motivation;
    if (hypothalamus_derive_motivation(&drives, &motivation) != 0) {
        return -1;
    }

    /* Blend drive states into content_intent (dims 400-410) */
    const float w_drives = 0.19f;

    if (cs->dim > 410) {
        cs->content_intent[400] += w_drives * drives.hunger;
        cs->content_intent[401] += w_drives * drives.thirst;
        cs->content_intent[402] += w_drives * drives.fatigue;
        cs->content_intent[403] += w_drives * drives.stress;
        cs->content_intent[404] += w_drives * drives.temperature;
        cs->content_intent[405] += w_drives * drives.arousal;
        cs->content_intent[406] += w_drives * drives.circadian_phase;
        cs->content_intent[407] += w_drives * motivation.drive_intensity;
        cs->content_intent[408] += w_drives * motivation.goal_urgency;
        cs->content_intent[409] += w_drives * motivation.affective_valence;
    }

    /* Urgency modulation: high drive intensity amplifies entire content_intent */
    if (motivation.drive_intensity > 0.6f) {
        float urgency_boost = 1.0f + 0.3f * (motivation.drive_intensity - 0.6f);
        for (uint32_t i = 0; i < cs->dim; i++) {
            cs->content_intent[i] *= urgency_boost;
        }
    }

    NIMCP_LOGGING_DEBUG("Hypothalamus integration: dominant_drive='%s' intensity=%.3f "
                        "urgency=%.3f valence=%.3f",
                        motivation.dominant_drive, motivation.drive_intensity,
                        motivation.goal_urgency, motivation.affective_valence);

    return 0;
}

/**
 * @brief Query whether hypothalamus coupling is enabled
 *
 * @param gl  Grounded language instance
 * @return true if hypothalamus integration is active
 */
bool grounded_language_hypothalamus_enabled(const grounded_language_t* gl) {
    if (!gl) return false;
    /* For now, always enabled when brainstem is present.
     * Future: add runtime flag to grounded_language struct. */
    return true;
}
