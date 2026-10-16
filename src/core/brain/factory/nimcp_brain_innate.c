/**
 * @file nimcp_brain_innate.c
 * @brief Innate Circuit Hardwiring — pre-configured infant capabilities
 *
 * WHAT: Implements brain_innate_hardwire() — sets up biologically-inspired
 *       innate circuits by modifying neuron biases and connection weights.
 * WHY:  Athena starts with infant-like capabilities, not as a blank slate.
 * HOW:  Divides neurons into functional regions and sets specific weight patterns.
 *
 * NEURON LAYOUT:
 *   The brain's hidden neurons are partitioned into functional regions:
 *   - Region 0 (0-10%):   Sensory input processing
 *   - Region 1 (10-20%):  Face/voice detection (innate biases here)
 *   - Region 2 (20-30%):  Motion detection
 *   - Region 3 (30-40%):  Reflex arcs
 *   - Region 4 (40-50%):  Vocalization/motor
 *   - Region 5 (50-60%):  Reward/value
 *   - Region 6 (60-70%):  Habituation/novelty
 *   - Region 7 (70-100%): General purpose (learns through experience)
 */

#define LOG_MODULE "INNATE"
#define LOG_MODULE_ID 0x0F00

#include "core/brain/factory/nimcp_brain_innate.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/neuralnet/nimcp_neuralnet_internal.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <string.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define INNATE_FACE_BIAS_WEIGHT     0.15f
#define INNATE_VOICE_BIAS_WEIGHT    0.12f
#define INNATE_MOTION_BIAS_WEIGHT   0.10f
#define INNATE_REFLEX_WEIGHT        0.30f
#define INNATE_CRY_WEIGHT           0.20f
#define INNATE_SOCIAL_REWARD_WEIGHT 0.08f
#define INNATE_HABITUATION_DECAY    0.95f
#define INNATE_NOVELTY_BOOST_WEIGHT 0.10f

/* Region boundaries as fractions of total hidden neurons */
#define REGION_SENSORY_START    0.00f
#define REGION_SENSORY_END      0.10f
#define REGION_FACE_VOICE_START 0.10f
#define REGION_FACE_VOICE_END   0.20f
#define REGION_MOTION_START     0.20f
#define REGION_MOTION_END       0.30f
#define REGION_REFLEX_START     0.30f
#define REGION_REFLEX_END       0.40f
#define REGION_VOCAL_START      0.40f
#define REGION_VOCAL_END        0.50f
#define REGION_REWARD_START     0.50f
#define REGION_REWARD_END       0.60f
#define REGION_HABIT_START      0.60f
#define REGION_HABIT_END        0.70f

/* ============================================================================
 * Helpers
 * ============================================================================ */

static void get_region_bounds(uint32_t num_hidden, float start_frac, float end_frac,
                               uint32_t* out_start, uint32_t* out_end) {
    *out_start = (uint32_t)(start_frac * num_hidden);
    *out_end   = (uint32_t)(end_frac * num_hidden);
    if (*out_end > num_hidden) *out_end = num_hidden;
    if (*out_start >= *out_end) *out_start = *out_end;
}

/* ============================================================================
 * Stage Configuration
 * ============================================================================ */

innate_config_t innate_default_config(innate_stage_t stage) {
    innate_config_t config = {
        .stage = stage,
        .enable_face_bias = true,
        .enable_voice_bias = true,
        .enable_motion_bias = false,
        .enable_reflexes = true,
        .enable_cry = true,
        .enable_social_reward = true,
        .enable_habituation = true,
        .enable_novelty = true,
        .bias_strength = 0.5f
    };

    switch (stage) {
        case INNATE_STAGE_NEWBORN:
            /* Reflexes dominant, minimal sensory processing */
            config.enable_motion_bias = false;
            config.bias_strength = 0.3f;
            break;
        case INNATE_STAGE_INFANT:
            /* Object tracking, voice recognition, social smile */
            config.enable_motion_bias = true;
            config.bias_strength = 0.5f;
            break;
        case INNATE_STAGE_CRAWLER:
            /* Object permanence, babbling, imitation */
            config.enable_motion_bias = true;
            config.bias_strength = 0.6f;
            break;
        case INNATE_STAGE_TODDLER:
            /* First words, tool use — innate biases start to fade */
            config.bias_strength = 0.4f;
            break;
        case INNATE_STAGE_CHILD:
            /* Grammar, theory of mind — mostly learned, minimal innate */
            config.bias_strength = 0.2f;
            config.enable_reflexes = false;
            config.enable_cry = false;
            break;
        default:
            break;
    }

    return config;
}

const char* innate_stage_name(innate_stage_t stage) {
    switch (stage) {
        case INNATE_STAGE_NEWBORN: return "newborn";
        case INNATE_STAGE_INFANT:  return "infant";
        case INNATE_STAGE_CRAWLER: return "crawler";
        case INNATE_STAGE_TODDLER: return "toddler";
        case INNATE_STAGE_CHILD:   return "child";
        default:                   return "unknown";
    }
}

/* ============================================================================
 * Circuit: Face Attention Bias
 * ============================================================================
 *
 * BIOLOGY: Neonates preferentially orient to face-like stimuli within hours
 * of birth (Johnson et al., 1991). The subcortical face detection pathway
 * (superior colliculus → pulvinar → amygdala) is functional at birth.
 *
 * IMPLEMENTATION: Boost biases of face/voice region neurons to make them
 * more responsive to structured input patterns. Set positive self-excitation
 * to create attractor dynamics for face-like patterns.
 */
static void hardwire_face_bias(brain_t brain, neural_network_t net,
                                uint32_t num_hidden, float strength) {
    uint32_t start, end;
    get_region_bounds(num_hidden, REGION_FACE_VOICE_START, REGION_FACE_VOICE_END, &start, &end);

    /* Offset by input neurons to get actual neuron IDs */
    uint32_t input_offset = brain->config.num_inputs;
    uint32_t count = 0;

    for (uint32_t i = start; i < end; i++) {
        uint32_t nid = input_offset + i;
        neuron_t* neuron = neural_network_get_neuron(net, nid);
        if (!neuron) continue;

        /* Lower threshold — more sensitive to input */
        neuron->threshold *= (1.0f - 0.3f * strength);

        /* Positive bias — tendency to activate */
        neuron->bias += INNATE_FACE_BIAS_WEIGHT * strength;

        /* Even-indexed neurons: face-tuned (spatial frequency bias)
         * Odd-indexed neurons: voice-tuned (temporal frequency bias) */
        if ((i % 2) == 0) {
            neuron->bias += 0.02f * strength;  /* Slightly more face-biased */
        }
        count++;
    }

    LOG_MODULE_INFO(LOG_MODULE, "Face attention bias: %u neurons, strength=%.2f", count, strength);
}

/* ============================================================================
 * Circuit: Voice Attention Bias
 * ============================================================================
 *
 * BIOLOGY: Neonates prefer human speech over other sounds, especially
 * their mother's voice (DeCasper & Fifer, 1980). The temporal voice area
 * (TVA) shows voice-selective responses in infants.
 */
static void hardwire_voice_bias(brain_t brain, neural_network_t net,
                                 uint32_t num_hidden, float strength) {
    uint32_t start, end;
    get_region_bounds(num_hidden, REGION_FACE_VOICE_START, REGION_FACE_VOICE_END, &start, &end);

    uint32_t input_offset = brain->config.num_inputs;
    uint32_t count = 0;

    for (uint32_t i = start; i < end; i++) {
        /* Odd-indexed neurons are voice-tuned */
        if ((i % 2) != 1) continue;

        uint32_t nid = input_offset + i;
        neuron_t* neuron = neural_network_get_neuron(net, nid);
        if (!neuron) continue;

        neuron->threshold *= (1.0f - 0.25f * strength);
        neuron->bias += INNATE_VOICE_BIAS_WEIGHT * strength;
        count++;
    }

    LOG_MODULE_INFO(LOG_MODULE, "Voice attention bias: %u neurons, strength=%.2f", count, strength);
}

/* ============================================================================
 * Circuit: Biological Motion Bias
 * ============================================================================
 *
 * BIOLOGY: Infants from 2 days old preferentially attend to biological
 * motion point-light displays over scrambled controls (Simion et al., 2008).
 */
static void hardwire_motion_bias(brain_t brain, neural_network_t net,
                                  uint32_t num_hidden, float strength) {
    uint32_t start, end;
    get_region_bounds(num_hidden, REGION_MOTION_START, REGION_MOTION_END, &start, &end);

    uint32_t input_offset = brain->config.num_inputs;
    uint32_t count = 0;

    for (uint32_t i = start; i < end; i++) {
        uint32_t nid = input_offset + i;
        neuron_t* neuron = neural_network_get_neuron(net, nid);
        if (!neuron) continue;

        /* Motion neurons need temporal sensitivity */
        neuron->threshold *= (1.0f - 0.2f * strength);
        neuron->bias += INNATE_MOTION_BIAS_WEIGHT * strength;

        /* Slightly faster adaptation — motion is transient */
        neuron->adaptation *= (1.0f + 0.1f * strength);
        count++;
    }

    LOG_MODULE_INFO(LOG_MODULE, "Motion bias: %u neurons, strength=%.2f", count, strength);
}

/* ============================================================================
 * Circuit: Spinal Reflexes
 * ============================================================================
 *
 * BIOLOGY: Neonatal reflexes are hardwired sensorimotor arcs:
 * - Rooting: Touch cheek → turn head toward stimulus
 * - Palmar grasp: Touch palm → flex fingers
 * - Moro (startle): Sudden drop/loud noise → extend arms, then flex
 *
 * IMPLEMENTATION: Strong direct connections from sensory→reflex→motor
 * neurons with high weights (bypass cortical processing).
 */
static void hardwire_reflexes(brain_t brain, neural_network_t net,
                               uint32_t num_hidden, float strength) {
    uint32_t start, end;
    get_region_bounds(num_hidden, REGION_REFLEX_START, REGION_REFLEX_END, &start, &end);

    uint32_t input_offset = brain->config.num_inputs;
    uint32_t count = 0;

    for (uint32_t i = start; i < end; i++) {
        uint32_t nid = input_offset + i;
        neuron_t* neuron = neural_network_get_neuron(net, nid);
        if (!neuron) continue;

        /* Reflex neurons: low threshold, strong response */
        neuron->threshold *= (1.0f - 0.5f * strength);
        neuron->bias += INNATE_REFLEX_WEIGHT * strength;

        /* Short refractory — can fire repeatedly */
        neuron->refractory_period *= (1.0f - 0.3f * strength);
        count++;
    }

    LOG_MODULE_INFO(LOG_MODULE, "Reflex arcs: %u neurons, strength=%.2f", count, strength);
}

/* ============================================================================
 * Circuit: Cry Vocalization
 * ============================================================================
 *
 * BIOLOGY: Crying is the neonate's primary communication tool.
 * High distress (low reward, high prediction error) activates
 * motor patterns for vocalization via Broca's area precursors.
 */
static void hardwire_cry(brain_t brain, neural_network_t net,
                          uint32_t num_hidden, float strength) {
    uint32_t start, end;
    get_region_bounds(num_hidden, REGION_VOCAL_START, REGION_VOCAL_END, &start, &end);

    uint32_t input_offset = brain->config.num_inputs;
    uint32_t count = 0;

    for (uint32_t i = start; i < end; i++) {
        uint32_t nid = input_offset + i;
        neuron_t* neuron = neural_network_get_neuron(net, nid);
        if (!neuron) continue;

        /* Vocalization neurons: moderate threshold, strong output */
        neuron->bias += INNATE_CRY_WEIGHT * strength;
        count++;
    }

    LOG_MODULE_INFO(LOG_MODULE, "Cry vocalization: %u neurons, strength=%.2f", count, strength);
}

/* ============================================================================
 * Circuit: Social Reward
 * ============================================================================
 *
 * BIOLOGY: Face and voice detection trigger dopaminergic reward signals
 * (Grossmann et al., 2008). This creates intrinsic motivation for
 * social interaction — the foundation of social learning.
 */
static void hardwire_social_reward(brain_t brain, neural_network_t net,
                                    uint32_t num_hidden, float strength) {
    uint32_t start, end;
    get_region_bounds(num_hidden, REGION_REWARD_START, REGION_REWARD_END, &start, &end);

    uint32_t input_offset = brain->config.num_inputs;
    uint32_t count = 0;

    for (uint32_t i = start; i < end; i++) {
        uint32_t nid = input_offset + i;
        neuron_t* neuron = neural_network_get_neuron(net, nid);
        if (!neuron) continue;

        /* Reward neurons: positive bias creates tonic activity */
        neuron->bias += INNATE_SOCIAL_REWARD_WEIGHT * strength;

        /* Low threshold — easily activated by social stimuli */
        neuron->threshold *= (1.0f - 0.2f * strength);
        count++;
    }

    LOG_MODULE_INFO(LOG_MODULE, "Social reward: %u neurons, strength=%.2f", count, strength);
}

/* ============================================================================
 * Circuit: Habituation & Novelty
 * ============================================================================
 *
 * BIOLOGY: Habituation is one of the most basic forms of learning present
 * at birth. Repeated stimuli → decreased attention (habituation).
 * Novel stimuli → increased attention (dishabituation/novelty preference).
 * Thompson & Spencer (1966) model: exponential decay of response.
 */
static void hardwire_habituation_novelty(brain_t brain, neural_network_t net,
                                          uint32_t num_hidden, float strength) {
    uint32_t start, end;
    get_region_bounds(num_hidden, REGION_HABIT_START, REGION_HABIT_END, &start, &end);

    uint32_t input_offset = brain->config.num_inputs;
    uint32_t count = 0;

    for (uint32_t i = start; i < end; i++) {
        uint32_t nid = input_offset + i;
        neuron_t* neuron = neural_network_get_neuron(net, nid);
        if (!neuron) continue;

        /* First half: habituation neurons (high adaptation) */
        if (i < (start + end) / 2) {
            neuron->adaptation += 0.3f * strength;  /* Faster adaptation = faster habituation */
            neuron->bias -= 0.02f * strength;       /* Slight negative bias = tends to suppress */
        } else {
            /* Second half: novelty neurons (low adaptation, high sensitivity) */
            neuron->adaptation *= (1.0f - 0.2f * strength);  /* Slower adaptation */
            neuron->bias += INNATE_NOVELTY_BOOST_WEIGHT * strength;
            neuron->threshold *= (1.0f - 0.3f * strength);  /* Very sensitive */
        }
        count++;
    }

    LOG_MODULE_INFO(LOG_MODULE, "Habituation/novelty: %u neurons, strength=%.2f", count, strength);
}

/* ============================================================================
 * Public API: brain_innate_hardwire()
 * ============================================================================ */

int brain_innate_hardwire(brain_t brain, const innate_config_t* config) {
    if (!brain || !config) return -1;
    if (!brain->network) {
        LOG_MODULE_ERROR(LOG_MODULE, "Cannot hardwire innate circuits: no network");
        return -1;
    }

    neural_network_t net = adaptive_network_get_base_network(brain->network);
    if (!net) {
        LOG_MODULE_ERROR(LOG_MODULE, "Cannot hardwire innate circuits: no base network");
        return -1;
    }

    uint32_t total_neurons = neural_network_get_num_neurons(net);
    uint32_t num_inputs = brain->config.num_inputs;
    uint32_t num_outputs = brain->config.num_outputs;

    /* Hidden neurons are between input and output layers */
    if (total_neurons <= num_inputs + num_outputs) {
        LOG_MODULE_ERROR(LOG_MODULE, "Not enough neurons for innate circuits "
                         "(total=%u, inputs=%u, outputs=%u)", total_neurons, num_inputs, num_outputs);
        return -1;
    }
    uint32_t num_hidden = total_neurons - num_inputs - num_outputs;

    float strength = config->bias_strength;
    if (strength < 0.0f) strength = 0.0f;
    if (strength > 1.0f) strength = 1.0f;

    LOG_MODULE_INFO(LOG_MODULE, "Hardwiring innate circuits: stage=%s, hidden=%u, strength=%.2f",
                    innate_stage_name(config->stage), num_hidden, strength);

    if (config->enable_face_bias) {
        hardwire_face_bias(brain, net, num_hidden, strength);
    }
    if (config->enable_voice_bias) {
        hardwire_voice_bias(brain, net, num_hidden, strength);
    }
    if (config->enable_motion_bias) {
        hardwire_motion_bias(brain, net, num_hidden, strength);
    }
    if (config->enable_reflexes) {
        hardwire_reflexes(brain, net, num_hidden, strength);
    }
    if (config->enable_cry) {
        hardwire_cry(brain, net, num_hidden, strength);
    }
    if (config->enable_social_reward) {
        hardwire_social_reward(brain, net, num_hidden, strength);
    }
    if (config->enable_habituation || config->enable_novelty) {
        hardwire_habituation_novelty(brain, net, num_hidden, strength);
    }

    /* Invalidate GPU weight cache since we modified neuron parameters */
    adaptive_network_invalidate_gpu_structure(brain->network);

    LOG_MODULE_INFO(LOG_MODULE, "Innate circuit hardwiring complete for stage: %s",
                    innate_stage_name(config->stage));

    return 0;
}
