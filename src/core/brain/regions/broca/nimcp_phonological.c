/**
 * @file nimcp_phonological.c
 * @brief Implementation of phonological processing for Broca's region
 *
 * @version Phase B1: Broca's Region Phonological Processing
 * @date 2025-11-22
 */

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

// Logging integration
#include "utils/logging/nimcp_logging.h"

// Unified memory integration
#include "utils/memory/nimcp_unified_memory.h"

#include "core/brain/regions/broca/nimcp_phonological.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "BROCA_PHONO"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(phonological, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// INTERNAL STRUCTURES
//=============================================================================

/**
 * @brief Internal processor state
 */
struct phonological_processor {
    /* Configuration */
    phonological_config_t config;

    /* Phoneme buffer */
    phoneme_t* phoneme_buffer;
    uint32_t phoneme_count;
    uint32_t phoneme_capacity;

    /* Syllable buffer */
    syllable_t* syllable_buffer;
    uint32_t syllable_count;
    uint32_t syllable_capacity;

    /* Prosody */
    prosody_curve_t prosody;
    bool prosody_generated;

    /* Status */
    phonological_status_t status;

    /* Statistics */
    uint32_t total_phonemes_processed;
    uint32_t total_syllables_generated;
};

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Check if phoneme is a vowel
 */
static bool is_vowel(uint8_t category) {
    return (category == PHONEME_CATEGORY_VOWEL);
}

/**
 * @brief Check if phoneme is a consonant
 */
static bool is_consonant(uint8_t category) {
    return (category == PHONEME_CATEGORY_CONSONANT);
}

/**
 * @brief Auto-detect phoneme category from symbol
 *
 * WHAT: Determine phoneme category from IPA-like symbol
 * WHY:  Simplify API by not requiring explicit category
 * HOW:  Pattern matching on common phoneme symbols
 *
 * @param symbol Phoneme symbol (ASCII approximation of IPA)
 * @return Detected category
 */
static uint8_t detect_phoneme_category(uint8_t symbol) {
    /* Vowels (standard 5 + common variants) */
    switch (symbol) {
        /* Primary vowels */
        case 'a': case 'e': case 'i': case 'o': case 'u':
        /* Extended vowels (schwa, etc.) */
        case '@':  /* schwa */
        case 'A':  /* open-a */
        case 'E':  /* epsilon */
        case 'I':  /* small-cap I */
        case 'O':  /* open-o */
        case 'U':  /* upsilon */
        case 'y':  /* front rounded */
        case '3':  /* open-mid central (SAMPA) */
            return PHONEME_CATEGORY_VOWEL;

        /* Semivowels / glides */
        case 'w': case 'j':
            return PHONEME_CATEGORY_SEMIVOWEL;

        /* Silence / pause markers */
        case ' ': case '_': case '.': case 0:
            return PHONEME_CATEGORY_SILENCE;

        /* All other characters are consonants */
        default:
            return PHONEME_CATEGORY_CONSONANT;
    }
}

/**
 * @brief Get sonority level (for syllabification)
 *
 * Higher sonority = more vowel-like
 * Sonority hierarchy: vowels > semivowels > liquids > nasals > fricatives > stops
 */
static int get_sonority(uint8_t category) {
    switch (category) {
        case PHONEME_CATEGORY_VOWEL: return 5;
        case PHONEME_CATEGORY_SEMIVOWEL: return 4;
        case PHONEME_CATEGORY_CONSONANT: return 2;  /* Simplified */
        case PHONEME_CATEGORY_SILENCE: return 0;
        default: return 1;
    }
}

/**
 * @brief Apply intonation pattern to generate F0 contour
 */
static void apply_intonation_pattern(prosody_curve_t* prosody,
                                     intonation_pattern_t pattern,
                                     uint32_t num_phonemes) {
    if (!prosody || num_phonemes == 0) return;

    prosody->pattern = pattern;
    prosody->num_points = num_phonemes;
    float baseline = prosody->baseline_f0;
    float range = prosody->f0_range;

    for (uint32_t i = 0; i < num_phonemes; i++) {
        float t = (num_phonemes > 1) ? (float)i / (float)(num_phonemes - 1) : 0.0F;  /* Normalized position [0-1] */

        switch (pattern) {
            case INTONATION_PATTERN_FLAT:
                prosody->f0_values[i] = baseline;
                break;

            case INTONATION_PATTERN_RISING:
                /* Linear rise */
                prosody->f0_values[i] = baseline + range * t;
                break;

            case INTONATION_PATTERN_FALLING:
                /* Linear fall */
                prosody->f0_values[i] = baseline + range * (1.0F - t);
                break;

            case INTONATION_PATTERN_RISE_FALL:
                /* Rise to peak at 50%, then fall */
                if (t < 0.5F) {
                    prosody->f0_values[i] = baseline + range * (2.0F * t);
                } else {
                    prosody->f0_values[i] = baseline + range * (2.0F * (1.0F - t));
                }
                break;

            case INTONATION_PATTERN_FALL_RISE:
                /* Fall to trough at 50%, then rise */
                if (t < 0.5F) {
                    prosody->f0_values[i] = baseline - range * (2.0F * t);
                } else {
                    prosody->f0_values[i] = baseline - range * (2.0F * (1.0F - t));
                }
                break;

            default:
                prosody->f0_values[i] = baseline;
                break;
        }

        /* Clamp to valid F0 range */
        prosody->f0_values[i] = nimcp_clampf(prosody->f0_values[i],
                                      PROSODY_F0_MIN,
                                      PROSODY_F0_MAX);
    }
}

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

phonological_config_t phonological_default_config(void) {
    phonological_config_t config;
    config.max_phonemes = PHONOLOGICAL_DEFAULT_MAX_PHONEMES;
    config.max_syllables = PHONOLOGICAL_DEFAULT_MAX_SYLLABLES;
    config.stress_weight = 0.7F;
    config.enable_prosody = true;
    config.enable_coarticulation = true;
    config.default_f0 = PROSODY_F0_DEFAULT;
    return config;
}

phonological_processor_t* phonological_create(const phonological_config_t* config) {
    /* WHAT: Allocate and initialize phonological processor
     * WHY:  Central system for speech planning
     * HOW:  Allocate buffers, initialize state */

    phonological_processor_t* proc = (phonological_processor_t*)nimcp_calloc(1, sizeof(phonological_processor_t));
    if (!proc) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "phonological_create: failed to allocate processor");

        return NULL;

    }

    /* Set configuration */
    if (config) {
        proc->config = *config;
    } else {
        proc->config = phonological_default_config();
    }

    /* Allocate phoneme buffer */
    proc->phoneme_capacity = proc->config.max_phonemes;
    proc->phoneme_buffer = (phoneme_t*)nimcp_calloc(proc->phoneme_capacity, sizeof(phoneme_t));
    if (!proc->phoneme_buffer) {
        nimcp_free(proc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "phonological_create: proc->phoneme_buffer is NULL");
        return NULL;
    }

    /* Allocate syllable buffer */
    proc->syllable_capacity = proc->config.max_syllables;
    proc->syllable_buffer = (syllable_t*)nimcp_calloc(proc->syllable_capacity, sizeof(syllable_t));
    if (!proc->syllable_buffer) {
        nimcp_free(proc->phoneme_buffer);
        nimcp_free(proc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "phonological_create: proc->syllable_buffer is NULL");
        return NULL;
    }

    /* Initialize prosody */
    proc->prosody.baseline_f0 = proc->config.default_f0;
    proc->prosody.f0_range = 50.0F;  /* Default range: 50 Hz */
    proc->prosody.pattern = INTONATION_PATTERN_FLAT;
    proc->prosody.num_points = 0;
    proc->prosody_generated = false;

    /* Initialize status */
    proc->status = PHONOLOGICAL_STATUS_IDLE;
    proc->phoneme_count = 0;
    proc->syllable_count = 0;
    proc->total_phonemes_processed = 0;
    proc->total_syllables_generated = 0;

    return proc;
}

void phonological_destroy(phonological_processor_t* processor) {
    /* WHAT: Free processor resources
     * WHY:  Prevent memory leaks
     * HOW:  Free all allocated buffers and structure */

    if (!processor) return;

    if (processor->phoneme_buffer) {
        nimcp_free(processor->phoneme_buffer);
    }

    if (processor->syllable_buffer) {
        nimcp_free(processor->syllable_buffer);
    }

    nimcp_free(processor);
}

bool phonological_reset(phonological_processor_t* processor) {
    /* WHAT: Reset processor to initial state
     * WHY:  Prepare for new utterance
     * HOW:  Clear buffers, reset counters */

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }

    /* Clear buffers */
    memset(processor->phoneme_buffer, 0, processor->phoneme_capacity * sizeof(phoneme_t));
    memset(processor->syllable_buffer, 0, processor->syllable_capacity * sizeof(syllable_t));

    /* Reset counters */
    processor->phoneme_count = 0;
    processor->syllable_count = 0;
    processor->prosody_generated = false;
    processor->prosody.num_points = 0;

    /* Reset status */
    processor->status = PHONOLOGICAL_STATUS_IDLE;

    return true;
}

//=============================================================================
// PHONEME OPERATIONS
//=============================================================================

bool phonological_add_phoneme(phonological_processor_t* processor, uint8_t phoneme) {
    /* WHAT: Add phoneme to buffer with auto-detected category
     * WHY:  Build up phoneme sequence with correct categorization
     * HOW:  Detect category from symbol, add to buffer with defaults */

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }

    /* Check capacity */
    if (processor->phoneme_count >= processor->phoneme_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "phonological_add_phoneme: capacity exceeded");
        return false;
    }

    /* Auto-detect phoneme category from symbol */
    uint8_t category = detect_phoneme_category(phoneme);

    /* Determine default duration based on category */
    float duration_ms;
    float voicing;
    switch (category) {
        case PHONEME_CATEGORY_VOWEL:
            duration_ms = 100.0F;  /* Vowels are longer */
            voicing = 1.0F;        /* Vowels are voiced */
            break;
        case PHONEME_CATEGORY_SEMIVOWEL:
            duration_ms = 70.0F;
            voicing = 1.0F;
            break;
        case PHONEME_CATEGORY_CONSONANT:
            duration_ms = 80.0F;
            /* Detect voicing for common consonants */
            voicing = (phoneme == 'b' || phoneme == 'd' || phoneme == 'g' ||
                      phoneme == 'v' || phoneme == 'z' || phoneme == 'm' ||
                      phoneme == 'n' || phoneme == 'l' || phoneme == 'r') ? 1.0F : 0.0F;
            break;
        case PHONEME_CATEGORY_SILENCE:
            duration_ms = 50.0F;
            voicing = 0.0F;
            break;
        default:
            duration_ms = 80.0F;
            voicing = 0.5F;
            break;
    }

    /* Add phoneme to buffer */
    phoneme_t* p = &processor->phoneme_buffer[processor->phoneme_count];
    p->symbol = phoneme;
    p->category = category;
    p->duration_ms = duration_ms;
    p->voicing = voicing;
    p->is_stressed = false;

    processor->phoneme_count++;
    processor->total_phonemes_processed++;

    /* Update status */
    if (processor->status == PHONOLOGICAL_STATUS_IDLE) {
        processor->status = PHONOLOGICAL_STATUS_BUFFERING;
    }

    return true;
}

bool phonological_add_phoneme_detailed(phonological_processor_t* processor,
                                       uint8_t phoneme,
                                       uint8_t category,
                                       float duration_ms,
                                       float voicing) {
    /* WHAT: Add phoneme with full details
     * WHY:  Fine-grained control over phoneme properties
     * HOW:  Populate full phoneme_t structure */

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }

    /* Check capacity */
    if (processor->phoneme_count >= processor->phoneme_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "phonological_add_phoneme: capacity exceeded");
        return false;
    }

    /* Validate inputs */
    duration_ms = nimcp_clampf(duration_ms, 10.0F, 500.0F);  /* 10-500ms */
    voicing = nimcp_clampf(voicing, 0.0F, 1.0F);

    /* Add phoneme */
    phoneme_t* p = &processor->phoneme_buffer[processor->phoneme_count];
    p->symbol = phoneme;
    p->category = category;
    p->duration_ms = duration_ms;
    p->voicing = voicing;
    p->is_stressed = false;

    processor->phoneme_count++;
    processor->total_phonemes_processed++;

    /* Update status */
    if (processor->status == PHONOLOGICAL_STATUS_IDLE) {
        processor->status = PHONOLOGICAL_STATUS_BUFFERING;
    }

    return true;
}

uint32_t phonological_get_phoneme_count(const phonological_processor_t* processor) {
    if (!processor) return 0;
    return processor->phoneme_count;
}

bool phonological_get_phoneme(const phonological_processor_t* processor,
                              uint32_t index,
                              phoneme_t* output) {
    /* WHAT: Retrieve phoneme from buffer by index
     * WHY:  Enable iteration over phoneme sequence
     * HOW:  Bounds check and copy */

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "output is NULL");
        return false;
    }
    if (index >= processor->phoneme_count) return false;

    *output = processor->phoneme_buffer[index];
    return true;
}

bool phonological_clear_phonemes(phonological_processor_t* processor) {
    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }

    memset(processor->phoneme_buffer, 0, processor->phoneme_capacity * sizeof(phoneme_t));
    processor->phoneme_count = 0;
    processor->status = PHONOLOGICAL_STATUS_IDLE;

    return true;
}

//=============================================================================
// SYLLABLE OPERATIONS
//=============================================================================

bool phonological_generate_syllables(phonological_processor_t* processor) {
    /* WHAT: Segment phonemes into syllables
     * WHY:  Syllables are basic unit of speech production
     * HOW:  Apply syllabification rules (onset maximization) */

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (processor->phoneme_count == 0) return false;

    processor->status = PHONOLOGICAL_STATUS_SYLLABIFYING;
    processor->syllable_count = 0;

    uint32_t i = 0;
    while (i < processor->phoneme_count && processor->syllable_count < processor->syllable_capacity) {
        syllable_t* syll = &processor->syllable_buffer[processor->syllable_count];
        memset(syll, 0, sizeof(syllable_t));

        /* Build onset (initial consonant(s)) */
        syll->onset_count = 0;

        /* Check if we have consonants followed by a vowel, or isolated consonants */
        bool has_upcoming_vowel = false;
        for (uint32_t lookahead = i; lookahead < processor->phoneme_count; lookahead++) {
            if (is_vowel(processor->phoneme_buffer[lookahead].category)) {
                has_upcoming_vowel = true;
                break;
            }
        }

        if (has_upcoming_vowel) {
            /* Normal onset building - collect consonants until we hit a vowel */
            while (i < processor->phoneme_count &&
                   is_consonant(processor->phoneme_buffer[i].category) &&
                   syll->onset_count < SYLLABLE_MAX_ONSET) {
                syll->onset[syll->onset_count++] = processor->phoneme_buffer[i++];
            }
        } else {
            /* No vowel ahead - each consonant becomes its own onset-only syllable */
            if (i < processor->phoneme_count &&
                is_consonant(processor->phoneme_buffer[i].category)) {
                syll->onset[syll->onset_count++] = processor->phoneme_buffer[i++];
            }
        }

        /* Build nucleus (vowel(s)) */
        syll->nucleus_count = 0;
        while (i < processor->phoneme_count &&
               is_vowel(processor->phoneme_buffer[i].category) &&
               syll->nucleus_count < SYLLABLE_MAX_NUCLEUS) {
            syll->nucleus[syll->nucleus_count++] = processor->phoneme_buffer[i++];
        }

        /* Handle consonant-only case (no nucleus) */
        if (syll->nucleus_count == 0) {
            if (syll->onset_count > 0) {
                syll->type = SYLLABLE_TYPE_ONSET_ONLY;
                /* Calculate duration for onset-only syllable */
                syll->duration_ms = 0.0F;
                for (uint32_t j = 0; j < syll->onset_count; j++) {
                    syll->duration_ms += syll->onset[j].duration_ms;
                }
                syll->is_initial = (processor->syllable_count == 0);
                syll->stress_level = STRESS_LEVEL_NONE;
                syll->pitch_f0 = processor->prosody.baseline_f0;
            }
            processor->syllable_count++;
            continue;
        }

        /* Build coda (final consonants) */
        syll->coda_count = 0;
        while (i < processor->phoneme_count &&
               is_consonant(processor->phoneme_buffer[i].category) &&
               syll->coda_count < SYLLABLE_MAX_CODA) {

            /* Stop if next phoneme is vowel (belongs to next syllable) */
            if (i + 1 < processor->phoneme_count &&
                is_vowel(processor->phoneme_buffer[i + 1].category)) {
                break;
            }

            syll->coda[syll->coda_count++] = processor->phoneme_buffer[i++];
        }

        /* Determine syllable type */
        if (syll->coda_count > 0) {
            syll->type = SYLLABLE_TYPE_CLOSED;
        } else {
            syll->type = SYLLABLE_TYPE_OPEN;
        }

        /* Calculate duration */
        syll->duration_ms = 0.0F;
        for (uint32_t j = 0; j < syll->onset_count; j++) {
            syll->duration_ms += syll->onset[j].duration_ms;
        }
        for (uint32_t j = 0; j < syll->nucleus_count; j++) {
            syll->duration_ms += syll->nucleus[j].duration_ms;
        }
        for (uint32_t j = 0; j < syll->coda_count; j++) {
            syll->duration_ms += syll->coda[j].duration_ms;
        }

        /* Mark position */
        syll->is_initial = (processor->syllable_count == 0);
        syll->is_final = false;  /* Will update after loop */

        /* Default stress and pitch */
        syll->stress_level = STRESS_LEVEL_NONE;
        syll->pitch_f0 = processor->prosody.baseline_f0;

        processor->syllable_count++;
    }

    /* Mark final syllable */
    if (processor->syllable_count > 0) {
        processor->syllable_buffer[processor->syllable_count - 1].is_final = true;
    }

    processor->total_syllables_generated += processor->syllable_count;
    /* Status remains SYLLABIFYING - changes to APPLYING_STRESS only when stress is applied */

    return true;
}

bool phonological_apply_stress(phonological_processor_t* processor,
                               uint32_t syllable_idx,
                               float stress_level) {
    /* WHAT: Set stress level for syllable
     * WHY:  Stress is critical for intelligibility
     * HOW:  Update syllable stress field, adjust duration */

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (syllable_idx >= processor->syllable_count) return false;

    /* Clamp stress level */
    stress_level = nimcp_clampf(stress_level, 0.0F, 1.0F);

    syllable_t* syll = &processor->syllable_buffer[syllable_idx];
    syll->stress_level = stress_level;

    /* Apply stress to phonemes in nucleus */
    for (uint32_t i = 0; i < syll->nucleus_count; i++) {
        syll->nucleus[i].is_stressed = (stress_level >= 0.5F);

        /* Stressed syllables are longer */
        if (stress_level > STRESS_LEVEL_NONE) {
            float duration_multiplier = 1.0F + (stress_level * processor->config.stress_weight);
            syll->nucleus[i].duration_ms *= duration_multiplier;
        }
    }

    /* Recalculate syllable duration */
    syll->duration_ms = 0.0F;
    for (uint32_t j = 0; j < syll->onset_count; j++) {
        syll->duration_ms += syll->onset[j].duration_ms;
    }
    for (uint32_t j = 0; j < syll->nucleus_count; j++) {
        syll->duration_ms += syll->nucleus[j].duration_ms;
    }
    for (uint32_t j = 0; j < syll->coda_count; j++) {
        syll->duration_ms += syll->coda[j].duration_ms;
    }

    return true;
}

uint32_t phonological_get_syllable_count(const phonological_processor_t* processor) {
    if (!processor) return 0;
    return processor->syllable_count;
}

bool phonological_get_syllable(const phonological_processor_t* processor,
                               uint32_t syllable_idx,
                               syllable_t* output) {
    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "output is NULL");
        return false;
    }
    if (syllable_idx >= processor->syllable_count) return false;

    *output = processor->syllable_buffer[syllable_idx];
    return true;
}

//=============================================================================
// PROSODY OPERATIONS
//=============================================================================

bool phonological_generate_prosody(phonological_processor_t* processor,
                                   intonation_pattern_t pattern) {
    /* WHAT: Generate pitch contour
     * WHY:  Prosody conveys meaning and emotion
     * HOW:  Apply intonation pattern to F0 values */

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!processor->config.enable_prosody) return false;
    if (processor->phoneme_count == 0) return false;

    apply_intonation_pattern(&processor->prosody, pattern, processor->phoneme_count);
    processor->prosody_generated = true;

    return true;
}

bool phonological_set_baseline_f0(phonological_processor_t* processor, float f0_hz) {
    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }

    /* Clamp to valid range */
    f0_hz = nimcp_clampf(f0_hz, PROSODY_F0_MIN, PROSODY_F0_MAX);

    processor->prosody.baseline_f0 = f0_hz;
    return true;
}

bool phonological_get_prosody(const phonological_processor_t* processor,
                              prosody_curve_t* output) {
    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "output is NULL");
        return false;
    }
    if (!processor->prosody_generated) return false;

    *output = processor->prosody;
    return true;
}

//=============================================================================
// COARTICULATION PLANNING
//=============================================================================

bool phonological_plan_coarticulation(phonological_processor_t* processor) {
    /* WHAT: Adjust phonemes for smooth transitions
     * WHY:  Natural speech has overlapping articulation
     * HOW:  Modify durations and features based on context */

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!processor->config.enable_coarticulation) return true;  /* Disabled is a valid no-op */
    if (processor->phoneme_count < 2) return true;  /* Nothing to coarticulate */

    /* Simple coarticulation: adjust boundaries between phonemes */
    for (uint32_t i = 0; i < processor->phoneme_count - 1; i++) {
        phoneme_t* current = &processor->phoneme_buffer[i];
        phoneme_t* next = &processor->phoneme_buffer[i + 1];

        /* Vowel-to-consonant: shorten vowel slightly */
        if (is_vowel(current->category) && is_consonant(next->category)) {
            current->duration_ms *= 0.95F;
        }

        /* Consonant-to-vowel: lengthen vowel slightly */
        if (is_consonant(current->category) && is_vowel(next->category)) {
            next->duration_ms *= 1.05F;
        }

        /* Voicing assimilation: adjacent consonants share voicing */
        if (is_consonant(current->category) && is_consonant(next->category)) {
            float avg_voicing = (current->voicing + next->voicing) / 2.0F;
            current->voicing = avg_voicing;
            next->voicing = avg_voicing;
        }
    }

    return true;
}

//=============================================================================
// STATUS AND QUERY
//=============================================================================

phonological_status_t phonological_get_status(const phonological_processor_t* processor) {
    if (!processor) return PHONOLOGICAL_STATUS_IDLE;
    return processor->status;
}

bool phonological_is_ready(const phonological_processor_t* processor) {
    /* WHAT: Check if ready for articulation
     * WHY:  Signal when processing is complete
     * HOW:  Check syllables generated and prosody applied (if enabled) */

    if (!processor) {
        return false;
    }

    bool syllables_ready = (processor->syllable_count > 0);
    bool prosody_ready = (!processor->config.enable_prosody || processor->prosody_generated);

    return syllables_ready && prosody_ready;
}

bool phonological_get_config(const phonological_processor_t* processor,
                             phonological_config_t* output) {
    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "output is NULL");
        return false;
    }

    *output = processor->config;
    return true;
}
