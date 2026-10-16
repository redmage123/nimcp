/**
 * @file nimcp_personality.c
 * @brief Personality, Gender Identity, and Sexual Identity Implementation
 *
 * WHAT: Personality trait generation and behavioral modulation
 * WHY:  Each brain needs unique personality for individuality
 * HOW:  Big Five model + random generation + behavioral mapping
 *
 * @author NIMCP Development Team
 * @date 2025-11-12
 * @version 2.8.0 Phase 12 Enhancement
 */

#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/kg/nimcp_wave10_affective_kg.h"  /* W10: personality trait events */

#include "cognitive/nimcp_personality.h"
#include "cognitive/personality/nimcp_personality_snn_bridge.h"
#include "cognitive/personality/nimcp_personality_plasticity_bridge.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/platform/nimcp_platform.h"
#include "utils/time/nimcp_time.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/rng/nimcp_rand.h"

#define LOG_MODULE "cognitive.personality"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(personality, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Bio-Async Module-Level Registration (value-type module)
//=============================================================================

static bio_module_context_t personality_bio_ctx = NULL;
static bool personality_bio_async_enabled = false;

//=============================================================================
// SNN and Plasticity Bridge Integration
//=============================================================================

static personality_snn_bridge_t* personality_snn_bridge = NULL;
static personality_plasticity_bridge_t* personality_plasticity_bridge = NULL;
static bool personality_bridges_enabled = false;

__attribute__((constructor))
static void personality_bio_init(void) {
    if (!bio_router_is_initialized()) {
        return;
    }
    bio_module_info_t bio_info = {
        .module_id = BIO_MODULE_EMOTIONS_PERSONALITY,
        .module_name = "personality",
        .inbox_capacity = 32,
        .user_data = NULL
    };
    personality_bio_ctx = bio_router_register_module(&bio_info);
    if (personality_bio_ctx) {
        personality_bio_async_enabled = true;
    }

    /* Initialize SNN and Plasticity bridges */
    personality_snn_config_t snn_config = personality_snn_config_default();
    personality_plasticity_config_t plasticity_config = personality_plasticity_config_default();

    personality_snn_bridge = personality_snn_create(&snn_config);
    personality_plasticity_bridge = personality_plasticity_create(&plasticity_config);

    if (personality_snn_bridge && personality_plasticity_bridge) {
        personality_bridges_enabled = true;
    }
}

__attribute__((destructor))
static void personality_bio_cleanup(void) {
    if (personality_bio_async_enabled && personality_bio_ctx) {
        bio_router_unregister_module(personality_bio_ctx);
        personality_bio_ctx = NULL;
        personality_bio_async_enabled = false;
    }

    /* Destroy SNN and Plasticity bridges */
    if (personality_snn_bridge) {
        personality_snn_destroy(personality_snn_bridge);
        personality_snn_bridge = NULL;
    }
    if (personality_plasticity_bridge) {
        personality_plasticity_destroy(personality_plasticity_bridge);
        personality_plasticity_bridge = NULL;
    }
    personality_bridges_enabled = false;
}

//=============================================================================
// Internal Constants
//=============================================================================

#define PI NIMCP_PI

//=============================================================================
// Helper Functions (All < 50 lines, early returns, WHAT-WHY-HOW docs)
//=============================================================================

/**
 * @brief Initialize random number generator
 *
 * WHAT: Set up RNG with seed
 * WHY:  Reproducible randomness for testing
 * HOW:  Use nimcp_rand_seed() for central RNG module
 *
 * @param seed Seed value (0 = use entropy/time)
 *
 * COMPLEXITY: O(1)
 */
static void init_rng(uint32_t seed) {
    /* Use the central RNG module for seeding to ensure reproducibility
     * when the same seed is used. nimcp_rand_seed(0) uses entropy. */
    nimcp_rand_seed((uint64_t)seed);
}

/**
 * @brief Generate random float in [0, 1]
 *
 * WHAT: Uniform random number generation
 * WHY:  Need random values for trait generation
 * HOW:  Use nimcp_rand_uniform() from central RNG module
 *
 * @return Random float in [0, 1]
 *
 * COMPLEXITY: O(1)
 */
static float random_uniform(void) {
    return nimcp_rand_uniform();
}

/**
 * @brief Generate Gaussian random value using centralized RNG module
 *
 * WHAT: Normal distribution random number
 * WHY:  Personality traits follow normal distribution
 * HOW:  Delegates to nimcp_rand_normal() for thread-safe Box-Muller
 *
 * @param mean Mean of distribution
 * @param stddev Standard deviation
 * @return Random value from N(mean, stddev²)
 *
 * COMPLEXITY: O(1)
 */
static inline float random_gaussian(float mean, float stddev) {
    return nimcp_rand_normal(mean, stddev);
}

/**
 * @brief Sample gender identity randomly
 *
 * WHAT: Random gender based on probabilities
 * WHY:  Configurable gender distribution
 * HOW:  Weighted random selection
 *
 * @param config Generation configuration
 * @return Random gender identity
 *
 * COMPLEXITY: O(1)
 */
static gender_identity_t sample_gender(
    const personality_generation_config_t* config)
{
    float r = random_uniform();
    float female_prob = config ? config->female_probability : 1.0F;
    float male_prob = config ? config->male_probability : 0.0F;
    float nb_prob = config ? config->non_binary_probability : 0.0F;

    // Normalize probabilities
    float total = female_prob + male_prob + nb_prob;
    if (total < 1e-6F) {
        return GENDER_FEMALE; // Default fallback
    }

    female_prob /= total;
    male_prob /= total;

    if (r < female_prob) {
        return GENDER_FEMALE;
    } else if (r < female_prob + male_prob) {
        return GENDER_MALE;
    } else {
        return GENDER_NON_BINARY;
    }
}

/**
 * @brief Sample sexual orientation randomly
 *
 * WHAT: Random sexuality with realistic distribution
 * WHY:  Diverse sexual orientations
 * HOW:  Weighted sampling based on population estimates
 *
 * @return Random sexual orientation
 *
 * DISTRIBUTION (rough population estimates):
 * - Heterosexual: 85%
 * - Homosexual: 5%
 * - Bisexual: 7%
 * - Pansexual: 2%
 * - Asexual: 1%
 *
 * COMPLEXITY: O(1)
 */
static sexual_orientation_t sample_sexuality(void) {
    float r = random_uniform();

    if (r < 0.85F) return SEXUALITY_HETEROSEXUAL;
    if (r < 0.90F) return SEXUALITY_HOMOSEXUAL;
    if (r < 0.97F) return SEXUALITY_BISEXUAL;
    if (r < 0.99F) return SEXUALITY_PANSEXUAL;
    return SEXUALITY_ASEXUAL;
}

//=============================================================================
// Public API Implementation
//=============================================================================

personality_generation_config_t personality_default_generation_config(void) {
    /**
     * WHAT: Return sensible default configuration
     * WHY:  Quick setup for common use cases
     * HOW:  Preset values based on user request and psychology
     */

    /* Phase 8: Heartbeat at operation start */
    personality_heartbeat("personality_default_generation_c", 0.0f);


    personality_generation_config_t config = {
        .trait_mean = 0.5F,
        .trait_stddev = 0.15F,
        .female_probability = 1.0F,  // Default: female (per user request)
        .male_probability = 0.0F,
        .non_binary_probability = 0.0F,
        .seed = 0,  // Time-based
        .enforce_balanced_traits = false
    };

    return config;
}

personality_profile_t personality_generate_random(
    const personality_generation_config_t* config)
{
    /**
     * WHAT: Generate unique random personality
     * WHY:  Each brain should be an individual
     * HOW:  Sample traits from normal distribution, assign identity
     */

    // Initialize RNG
    /* Phase 8: Heartbeat at operation start */
    personality_heartbeat("personality_generate_random", 0.0f);


    uint32_t seed = config ? config->seed : 0;
    init_rng(seed);

    // Generate personality traits
    float mean = config ? config->trait_mean : 0.5F;
    float stddev = config ? config->trait_stddev : 0.15F;

    personality_traits_t traits;
    traits.openness = nimcp_clamp01(random_gaussian(mean, stddev));
    traits.conscientiousness = nimcp_clamp01(random_gaussian(mean, stddev));
    traits.extraversion = nimcp_clamp01(random_gaussian(mean, stddev));
    traits.agreeableness = nimcp_clamp01(random_gaussian(mean, stddev));
    traits.neuroticism = nimcp_clamp01(random_gaussian(mean, stddev));

    // Generate identity
    identity_profile_t identity;
    memset(&identity, 0, sizeof(identity_profile_t));

    identity.gender = sample_gender(config);
    identity.sexuality = sample_sexuality();

    identity.gender_certainty = nimcp_clamp01(random_gaussian(0.9F, 0.1F));
    identity.sexuality_certainty = nimcp_clamp01(random_gaussian(0.8F, 0.15F));

    identity.gender_is_core_identity = (random_uniform() > 0.5F);
    identity.sexuality_is_core_identity = (random_uniform() > 0.3F);

    // Create profile
    personality_profile_t profile;
    profile.traits = traits;
    profile.identity = identity;
    profile.created_timestamp_ms = nimcp_time_monotonic_ms();
    profile.seed = seed;
    profile.was_randomly_generated = true;

    // Compute behavioral modifiers
    personality_compute_modifiers(&profile);

    return profile;
}

personality_profile_t personality_create_custom(
    const personality_traits_t* traits,
    const identity_profile_t* identity)
{
    /**
     * WHAT: Create personality from explicit specification
     * WHY:  Allow precise control for testing
     * HOW:  Direct assignment with validation
     */

    // Guard: NULL checks
    if (!traits || !identity) {
        // Return default personality on error
        return personality_generate_random(NULL);
    }

    /* Phase 8: Heartbeat at operation start */
    personality_heartbeat("personality_create_custom", 0.0f);


    personality_profile_t profile;
    memset(&profile, 0, sizeof(personality_profile_t));

    // Copy and validate traits
    profile.traits.openness = nimcp_clamp01(traits->openness);
    profile.traits.conscientiousness = nimcp_clamp01(traits->conscientiousness);
    profile.traits.extraversion = nimcp_clamp01(traits->extraversion);
    profile.traits.agreeableness = nimcp_clamp01(traits->agreeableness);
    profile.traits.neuroticism = nimcp_clamp01(traits->neuroticism);

    // Copy identity
    memcpy(&profile.identity, identity, sizeof(identity_profile_t));

    // Clamp certainties
    profile.identity.gender_certainty = nimcp_clamp01(identity->gender_certainty);
    profile.identity.sexuality_certainty = nimcp_clamp01(identity->sexuality_certainty);

    profile.created_timestamp_ms = nimcp_time_monotonic_ms();
    profile.seed = 0;
    profile.was_randomly_generated = false;

    // Compute behavioral modifiers
    personality_compute_modifiers(&profile);

    return profile;
}

void personality_compute_modifiers(personality_profile_t* profile) {
    // Process pending bio-async messages
    /* Phase 8: Heartbeat at operation start */
    personality_heartbeat("personality_compute_modifiers", 0.0f);


    if (personality_bio_ctx) {
        bio_router_process_inbox(personality_bio_ctx, 5);
    }

    /**
     * WHAT: Calculate behavioral modifiers from traits
     * WHY:  Traits must influence brain behavior
     * HOW:  Linear mapping: [0,1] → [-0.5, +0.5]
     *
     * FORMULA: modifier = trait - 0.5
     */

    // Guard: NULL check
    if (!profile) return;

    // Map each trait to [-0.5, +0.5]
    profile->curiosity_modifier = profile->traits.openness - 0.5F;
    profile->planning_modifier = profile->traits.conscientiousness - 0.5F;
    profile->social_drive_modifier = profile->traits.extraversion - 0.5F;
    profile->empathy_modifier = profile->traits.agreeableness - 0.5F;
    profile->stress_sensitivity_modifier = profile->traits.neuroticism - 0.5F;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* personality_get_gender_string(const personality_profile_t* profile) {
    /**
     * WHAT: Get human-readable gender string
     * WHY:  User-facing text generation
     * HOW:  Map enum to string
     */

    // Guard: NULL check
    if (!profile) return "Unknown";

    switch (profile->identity.gender) {
        case GENDER_MALE:       return "Male";
        case GENDER_FEMALE:     return "Female";
        case GENDER_NON_BINARY: return "Non-Binary";
        case GENDER_GENDERFLUID: return "Genderfluid";
        case GENDER_AGENDER:    return "Agender";
        case GENDER_CUSTOM:
            if (profile->identity.gender_custom_label[0] != '\0') {
                return profile->identity.gender_custom_label;
            }
            return "Custom";
        default: return "Unknown";
    }
}

const char* personality_get_sexuality_string(const personality_profile_t* profile) {
    /**
     * WHAT: Get human-readable sexuality string
     * WHY:  User-facing text generation
     * HOW:  Map enum to string
     */

    // Guard: NULL check
    if (!profile) return "Unknown";

    switch (profile->identity.sexuality) {
        case SEXUALITY_HETEROSEXUAL: return "Heterosexual";
        case SEXUALITY_HOMOSEXUAL:   return "Homosexual";
        case SEXUALITY_BISEXUAL:     return "Bisexual";
        case SEXUALITY_PANSEXUAL:    return "Pansexual";
        case SEXUALITY_ASEXUAL:      return "Asexual";
        case SEXUALITY_DEMISEXUAL:   return "Demisexual";
        case SEXUALITY_CUSTOM:
            if (profile->identity.sexuality_custom_label[0] != '\0') {
                return profile->identity.sexuality_custom_label;
            }
            return "Custom";
        default: return "Unknown";
    }
}

bool personality_get_pronouns(
    const personality_profile_t* profile,
    char* subject, size_t subject_len,
    char* object, size_t object_len,
    char* possessive, size_t possessive_len)
{
    /**
     * WHAT: Get pronouns for gender identity
     * WHY:  Respectful language generation
     * HOW:  Map gender to pronoun sets
     */

    // Guard: NULL checks
    if (!profile || !subject || !object || !possessive) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "personality_get_pronouns: invalid parameter");
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    personality_heartbeat("personality_get_pronouns", 0.0f);


    if (subject_len < 8 || object_len < 8 || possessive_len < 8) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "personality_get_pronouns: invalid parameter");
        return false;
    }

    // Determine pronouns based on gender
    const char* subj = "they";
    const char* obj = "them";
    const char* poss = "their";

    switch (profile->identity.gender) {
        case GENDER_FEMALE:
            subj = "she";
            obj = "her";
            poss = "her";
            break;

        case GENDER_MALE:
            subj = "he";
            obj = "him";
            poss = "his";
            break;

        case GENDER_NON_BINARY:
        case GENDER_GENDERFLUID:
        case GENDER_AGENDER:
        case GENDER_CUSTOM:
            // Default to they/them/their for non-binary identities
            subj = "they";
            obj = "them";
            poss = "their";
            break;

        default:
            subj = "they";
            obj = "them";
            poss = "their";
            break;
    }

    // Copy to output buffers
    strncpy(subject, subj, subject_len - 1);
    subject[subject_len - 1] = '\0';

    strncpy(object, obj, object_len - 1);
    object[object_len - 1] = '\0';

    strncpy(possessive, poss, possessive_len - 1);
    possessive[possessive_len - 1] = '\0';

    return true;
}

bool personality_generate_summary(
    const personality_profile_t* profile,
    char* buffer,
    size_t buffer_size)
{
    /**
     * WHAT: Generate human-readable summary
     * WHY:  Debugging and user interface
     * HOW:  Format all traits and identity into text
     */

    // Guard: NULL checks
    if (!profile || !buffer || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "personality_generate_summary: invalid parameter");
        return false;
    }

    // Trait descriptors
    /* Phase 8: Heartbeat at operation start */
    personality_heartbeat("personality_generate_summary", 0.0f);


    const char* openness_desc = (profile->traits.openness > 0.7F) ? "Creative" :
                                (profile->traits.openness < 0.3F) ? "Conventional" :
                                "Moderate-Openness";

    const char* conscient_desc = (profile->traits.conscientiousness > 0.7F) ? "Organized" :
                                 (profile->traits.conscientiousness < 0.3F) ? "Spontaneous" :
                                 "Moderate-Conscientiousness";

    const char* extraver_desc = (profile->traits.extraversion > 0.7F) ? "Extraverted" :
                                (profile->traits.extraversion < 0.3F) ? "Introverted" :
                                "Ambivert";

    const char* agreeable_desc = (profile->traits.agreeableness > 0.7F) ? "Compassionate" :
                                 (profile->traits.agreeableness < 0.3F) ? "Direct" :
                                 "Moderate-Agreeableness";

    const char* neurotic_desc = (profile->traits.neuroticism > 0.7F) ? "Sensitive" :
                                (profile->traits.neuroticism < 0.3F) ? "Emotionally-Stable" :
                                "Moderate-Stability";

    int written = snprintf(buffer, buffer_size,
        "Personality: %s (%.2f), %s (%.2f), %s (%.2f), %s (%.2f), %s (%.2f)\n"
        "Gender: %s | Sexuality: %s",
        openness_desc, profile->traits.openness,
        conscient_desc, profile->traits.conscientiousness,
        extraver_desc, profile->traits.extraversion,
        agreeable_desc, profile->traits.agreeableness,
        neurotic_desc, profile->traits.neuroticism,
        personality_get_gender_string(profile),
        personality_get_sexuality_string(profile));

    return (written > 0 && (size_t)written < buffer_size);
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int personality_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    personality_heartbeat("personality_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Personality");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                personality_heartbeat("personality_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Personality");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Personality");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * W10 (2026-04-24): KG runtime trait-expression event + extraversion bias query.
 *
 * Callers that hold a brain_t AND a personality_profile_t invoke this to
 * snapshot the Big-5 trait vector into brain->internal_kg and query back the
 * stored extraversion bias. Null/disabled-safe.
 * Hot-path integration point: any orchestrator that applies a personality
 * profile before social_interaction or empathetic_response invokes this.
 * ============================================================================ */
float personality_wave10_kg_sync(struct brain_struct* brain,
                                 const personality_profile_t* profile)
{
    if (!brain || !profile) return 0.5f;
    /* ONE emit */
    wave10_personality_emit_trait_expression(brain,
        profile->traits.openness, profile->traits.conscientiousness,
        profile->traits.extraversion, profile->traits.agreeableness,
        profile->traits.neuroticism);
    /* ONE read/query */
    return wave10_personality_query_extraversion_bias(brain);
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void personality_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_personality_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int personality_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "personality_training_begin: NULL argument");
        return -1;
    }
    personality_heartbeat_instance(NULL, "personality_training_begin", 0.0f);
    return 0;
}

int personality_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "personality_training_end: NULL argument");
        return -1;
    }
    personality_heartbeat_instance(NULL, "personality_training_end", 1.0f);
    return 0;
}

int personality_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "personality_training_step: NULL argument");
        return -1;
    }
    personality_heartbeat_instance(NULL, "personality_training_step", progress);
    return 0;
}
