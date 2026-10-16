/**
 * @file nimcp_cognitive_transcript.c
 * @brief Cognitive Transcript — implementation
 *
 * Provides the transcript data structure that captures all cognitive stage
 * outputs during brain_decide(), enabling rich response composition.
 */

#include "core/brain/nimcp_cognitive_transcript.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>
#include <math.h>

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cognitive_transcript)

/* Module name lookup table — must match transcript_module_t enum order */
static const char* const MODULE_NAMES[] = {
    [TRANSCRIPT_MODULE_NONE]             = "none",
    [TRANSCRIPT_MODULE_WELLBEING]        = "wellbeing",
    [TRANSCRIPT_MODULE_ENGRAM]           = "engram",
    [TRANSCRIPT_MODULE_CURIOSITY]        = "curiosity",
    [TRANSCRIPT_MODULE_SLEEP]            = "sleep",
    [TRANSCRIPT_MODULE_PREDICTIVE]       = "predictive",
    [TRANSCRIPT_MODULE_HEMISPHERIC]      = "hemispheric",
    [TRANSCRIPT_MODULE_WORKING_MEMORY]   = "working_memory",
    [TRANSCRIPT_MODULE_SEMANTIC_MEMORY]  = "semantic_memory",
    [TRANSCRIPT_MODULE_EPISODIC_MEMORY]  = "episodic_memory",
    [TRANSCRIPT_MODULE_REASONING]        = "reasoning",
    [TRANSCRIPT_MODULE_INNER_DIALOGUE]   = "inner_dialogue",
    [TRANSCRIPT_MODULE_IMAGINATION]      = "imagination",
    [TRANSCRIPT_MODULE_RECURSIVE_COG]    = "recursive_cognition",
    [TRANSCRIPT_MODULE_EXECUTIVE]        = "executive",
    [TRANSCRIPT_MODULE_GLOBAL_WORKSPACE] = "global_workspace",
    [TRANSCRIPT_MODULE_EMOTION]          = "emotion",
    [TRANSCRIPT_MODULE_THEORY_OF_MIND]   = "theory_of_mind",
    [TRANSCRIPT_MODULE_MIRROR_NEURON]    = "mirror_neuron",
    [TRANSCRIPT_MODULE_ETHICS]           = "ethics",
    [TRANSCRIPT_MODULE_EPISTEMIC]        = "epistemic",
    [TRANSCRIPT_MODULE_FEP_PARIETAL]     = "fep_parietal",
    [TRANSCRIPT_MODULE_PRED_HIERARCHY]   = "predictive_hierarchy",
    [TRANSCRIPT_MODULE_VAE]              = "vae",
    [TRANSCRIPT_MODULE_JEPA]             = "jepa",
    [TRANSCRIPT_MODULE_GROUNDED_LANG]    = "grounded_language",
    [TRANSCRIPT_MODULE_KNOWLEDGE]        = "knowledge",
    [TRANSCRIPT_MODULE_CREATIVE]         = "creative",
    [TRANSCRIPT_MODULE_INTUITION]        = "intuition",
};

cognitive_transcript_t* transcript_create(void)
{
    cognitive_transcript_t* t = nimcp_calloc(1, sizeof(cognitive_transcript_t));
    if (!t) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_MEMORY,
            "transcript_create: failed to allocate cognitive_transcript_t");
        return NULL;
    }

    bbb_register_module("cognitive_transcript", BBB_MODULE_TYPE_COGNITIVE);

    return t;
}

void transcript_free(cognitive_transcript_t* t)
{
    if (t) {
        nimcp_free(t);
    }
}

transcript_entry_t* transcript_add(cognitive_transcript_t* t,
                                   transcript_module_t module,
                                   float salience,
                                   float confidence,
                                   const char* summary)
{
    if (!t) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "transcript_add: null transcript");
        return NULL;
    }
    if (t->num_entries >= NIMCP_TRANSCRIPT_MAX_ENTRIES) {
        return NULL;
    }

    transcript_entry_t* entry = &t->entries[t->num_entries];
    entry->module = module;
    entry->contributed = true;
    entry->salience = salience;
    entry->confidence = confidence;
    entry->num_values = 0;
    entry->latency_us = 0;

    if (summary) {
        strncpy(entry->summary, summary, NIMCP_TRANSCRIPT_SUMMARY_LEN - 1);
        entry->summary[NIMCP_TRANSCRIPT_SUMMARY_LEN - 1] = '\0';
    } else {
        entry->summary[0] = '\0';
    }

    t->num_entries++;
    return entry;
}

void transcript_entry_add_value(transcript_entry_t* entry,
                                const char* label,
                                float value)
{
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "transcript_entry_add_value: null entry");
        return;
    }
    if (entry->num_values >= NIMCP_TRANSCRIPT_MAX_VALUES) {
        return;
    }
    entry->value_labels[entry->num_values] = label;
    entry->values[entry->num_values] = value;
    entry->num_values++;
}

void transcript_finalize(cognitive_transcript_t* t)
{
    if (!t) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "transcript_finalize: null transcript");
        return;
    }
    if (t->num_entries == 0) {
        return;
    }

    /* Find dominant module and compute aggregates */
    float max_salience = -1.0f;
    float sum_confidence = 0.0f;
    uint32_t contributed_count = 0;
    uint64_t total_latency = 0;

    for (uint32_t i = 0; i < t->num_entries; i++) {
        const transcript_entry_t* e = &t->entries[i];
        if (!e->contributed) continue;

        contributed_count++;
        sum_confidence += e->confidence;
        total_latency += e->latency_us;

        if (e->salience > max_salience) {
            max_salience = e->salience;
            t->dominant_module = e->module;
            t->dominant_salience = e->salience;
        }

        /* Set response composition hints based on module type */
        switch (e->module) {
            case TRANSCRIPT_MODULE_EMOTION:
                if (e->salience > 0.3f) t->has_emotional_content = true;
                break;
            case TRANSCRIPT_MODULE_ETHICS:
                if (e->confidence > 0.0f) t->has_ethical_concern = true;
                break;
            case TRANSCRIPT_MODULE_CREATIVE:
                if (e->salience > 0.3f) t->has_creative_insight = true;
                break;
            case TRANSCRIPT_MODULE_KNOWLEDGE:
            case TRANSCRIPT_MODULE_SEMANTIC_MEMORY:
                if (e->contributed) t->has_knowledge_retrieval = true;
                break;
            case TRANSCRIPT_MODULE_EPISTEMIC:
                if (e->confidence < 0.5f) t->has_uncertainty = true;
                break;
            case TRANSCRIPT_MODULE_REASONING:
                if (e->contributed) t->has_reasoning_chain = true;
                break;
            case TRANSCRIPT_MODULE_ENGRAM:
            case TRANSCRIPT_MODULE_EPISODIC_MEMORY:
                if (e->contributed) t->has_memory_recall = true;
                break;
            case TRANSCRIPT_MODULE_PREDICTIVE:
            case TRANSCRIPT_MODULE_PRED_HIERARCHY:
            case TRANSCRIPT_MODULE_JEPA:
                if (e->contributed) t->has_prediction = true;
                break;
            case TRANSCRIPT_MODULE_IMAGINATION:
                if (e->contributed) t->has_imagination = true;
                break;
            case TRANSCRIPT_MODULE_INNER_DIALOGUE:
                /* Check for conflict via low confidence */
                if (e->confidence < 0.4f) t->has_inner_conflict = true;
                break;
            default:
                break;
        }
    }

    t->total_latency_us = total_latency;

    /* Coherence: variance of confidences — low variance = high coherence */
    if (contributed_count > 1) {
        float mean_conf = sum_confidence / contributed_count;
        float var_sum = 0.0f;
        for (uint32_t i = 0; i < t->num_entries; i++) {
            if (!t->entries[i].contributed) continue;
            float diff = t->entries[i].confidence - mean_conf;
            var_sum += diff * diff;
        }
        float variance = var_sum / contributed_count;
        /* Map variance [0, 0.25] → coherence [1, 0] */
        t->overall_coherence = 1.0f - fminf(sqrtf(variance) * 2.0f, 1.0f);
    } else {
        t->overall_coherence = 1.0f;
    }

    /* Cognitive load: proportion of modules that contributed, weighted by latency */
    t->cognitive_load = (float)contributed_count / (float)TRANSCRIPT_MODULE_COUNT;
}

const char* transcript_module_name(transcript_module_t module)
{
    if (module < 0 || module >= TRANSCRIPT_MODULE_COUNT) {
        return "unknown";
    }
    return MODULE_NAMES[module];
}

const transcript_entry_t* transcript_find(const cognitive_transcript_t* t,
                                          transcript_module_t module)
{
    if (!t) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "transcript_find: null transcript");
        return NULL;
    }
    for (uint32_t i = 0; i < t->num_entries; i++) {
        if (t->entries[i].module == module) {
            return &t->entries[i];
        }
    }
    return NULL;
}

uint32_t transcript_get_by_salience(const cognitive_transcript_t* t,
                                    uint32_t* indices,
                                    uint32_t max_indices)
{
    if (!t) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "transcript_get_by_salience: null transcript");
        return 0;
    }
    if (!indices || t->num_entries == 0) {
        return 0;
    }

    uint32_t count = t->num_entries < max_indices ? t->num_entries : max_indices;

    /* Initialize indices */
    for (uint32_t i = 0; i < count; i++) {
        indices[i] = i;
    }

    /* Simple insertion sort by salience descending (small N) */
    for (uint32_t i = 1; i < count; i++) {
        uint32_t key = indices[i];
        float key_sal = t->entries[key].salience;
        int j = (int)i - 1;
        while (j >= 0 && t->entries[indices[j]].salience < key_sal) {
            indices[j + 1] = indices[j];
            j--;
        }
        indices[j + 1] = key;
    }

    return count;
}
