/**
 * @file nimcp_olfactory.c
 * @brief Olfactory Cortex Implementation
 * @version Phase 6: Sensory Processing
 * @date 2026-01-12
 */

#include "core/brain/regions/olfactory/nimcp_olfactory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdatomic.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(olfactory, MESH_ADAPTER_CATEGORY_COGNITIVE)


static uint64_t olfact_get_time_ms(void) {
    static _Atomic uint64_t counter = 0;
    return counter++;
}

olfact_config_t olfact_default_config(void) {
    olfact_config_t config = {
        .num_mitral_cells = OLFACT_DEFAULT_MITRAL_CELLS,
        .num_piriform_neurons = OLFACT_DEFAULT_PIRIFORM,
        .max_stored_odors = OLFACT_MAX_ODORS,
        .adaptation_rate = 0.001f,
        .lateral_inhibition_strength = 0.3f,
        .enable_sniff_coupling = true,
        .enable_emotional_memory = true,
        .enable_all_bridges = true
    };
    return config;
}

nimcp_olfactory_t* olfact_create(const olfact_config_t* config) {
    olfact_config_t default_config;
    if (!config) {
        default_config = olfact_default_config();
        config = &default_config;
    }

    nimcp_olfactory_t* olfact = (nimcp_olfactory_t*)nimcp_calloc(1, sizeof(nimcp_olfactory_t));
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "olfact_create: allocation failed");
        return NULL;
    }

    memcpy(&olfact->config, config, sizeof(olfact_config_t));
    olfact->status = OLFACT_STATUS_IDLE;
    olfact->last_error = OLFACT_ERROR_NONE;

    /* Allocate glomeruli */
    olfact->glomeruli = (olfact_glomerulus_t*)nimcp_calloc(OLFACT_MAX_GLOMERULI, sizeof(olfact_glomerulus_t));
    if (!olfact->glomeruli) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "olfact_create: glomeruli allocation failed");
        nimcp_free(olfact);
        return NULL;
    }
    olfact->num_glomeruli = OLFACT_MAX_GLOMERULI;

    /* Allocate mitral cells */
    olfact->mitral_cells = (olfact_mitral_cell_t*)nimcp_calloc(config->num_mitral_cells, sizeof(olfact_mitral_cell_t));
    if (!olfact->mitral_cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "olfact_create: mitral_cells allocation failed");
        nimcp_free(olfact->glomeruli);
        nimcp_free(olfact);
        return NULL;
    }
    olfact->num_mitral_cells = config->num_mitral_cells;

    /* Allocate piriform cortex */
    olfact->piriform_activation = (float*)nimcp_calloc(config->num_piriform_neurons, sizeof(float));
    if (!olfact->piriform_activation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "olfact_create: piriform_activation allocation failed");
        nimcp_free(olfact->mitral_cells);
        nimcp_free(olfact->glomeruli);
        nimcp_free(olfact);
        return NULL;
    }
    olfact->num_piriform = config->num_piriform_neurons;

    /* Allocate memories */
    olfact->max_memories = config->max_stored_odors;
    olfact->odor_memories = (olfact_memory_t*)nimcp_calloc(olfact->max_memories, sizeof(olfact_memory_t));
    if (!olfact->odor_memories) {
        nimcp_free(olfact->piriform_activation);
        nimcp_free(olfact->mitral_cells);
        nimcp_free(olfact->glomeruli);
        nimcp_free(olfact);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "olfact_create: odor_memories allocation failed");
        return NULL;
    }
    olfact->num_memories = 0;

    /* Allocate known odors */
    olfact->known_odors = (olfact_odor_id_t*)nimcp_calloc(config->max_stored_odors, sizeof(olfact_odor_id_t));
    if (!olfact->known_odors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "olfact_create: known_odors allocation failed");
        nimcp_free(olfact->odor_memories);
        nimcp_free(olfact->piriform_activation);
        nimcp_free(olfact->mitral_cells);
        nimcp_free(olfact->glomeruli);
        nimcp_free(olfact);
        return NULL;
    }
    olfact->num_known_odors = 0;

    /* Initialize sniff state */
    olfact->sniff_state.phase = SNIFF_PHASE_BASELINE;
    olfact->sniff_state.cycle_position = 0.0f;
    olfact->sniff_state.sniff_strength = 1.0f;

    olfact->adaptation_level = 0.0f;
    olfact->creation_time = olfact_get_time_ms();
    olfact->last_update_time = olfact->creation_time;
    olfact->status = OLFACT_STATUS_READY;

    return olfact;
}

void olfact_destroy(nimcp_olfactory_t* olfact) {
    if (!olfact) return;
    if (olfact->glomeruli) nimcp_free(olfact->glomeruli);
    if (olfact->mitral_cells) nimcp_free(olfact->mitral_cells);
    if (olfact->piriform_activation) nimcp_free(olfact->piriform_activation);
    if (olfact->odor_memories) nimcp_free(olfact->odor_memories);
    if (olfact->known_odors) nimcp_free(olfact->known_odors);
    nimcp_free(olfact);
}

int olfact_reset(nimcp_olfactory_t* olfact) {
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_reset: olfact is NULL");
        return -1;
    }
    memset(olfact->piriform_activation, 0, olfact->num_piriform * sizeof(float));
    olfact->adaptation_level = 0.0f;
    olfact->sniff_state.phase = SNIFF_PHASE_BASELINE;
    olfact->sniff_state.cycle_position = 0.0f;
    olfact->sniff_state.cycle_count = 0;
    olfact->updates_processed = 0;
    olfact->status = OLFACT_STATUS_READY;
    olfact->last_error = OLFACT_ERROR_NONE;
    return 0;
}

int olfact_update(nimcp_olfactory_t* olfact, float dt) {
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_update: olfact is NULL");
        return -1;
    }

    /* Update sniff cycle */
    if (olfact->config.enable_sniff_coupling) {
        olfact->sniff_state.cycle_position += dt / OLFACT_SNIFF_CYCLE_MS;
        if (olfact->sniff_state.cycle_position >= 1.0f) {
            olfact->sniff_state.cycle_position -= 1.0f;
            olfact->sniff_state.cycle_count++;
        }

        /* Update phase */
        if (olfact->sniff_state.cycle_position < 0.3f) {
            olfact->sniff_state.phase = SNIFF_PHASE_INSPIRATION;
        } else if (olfact->sniff_state.cycle_position < 0.5f) {
            olfact->sniff_state.phase = SNIFF_PHASE_PEAK;
        } else {
            olfact->sniff_state.phase = SNIFF_PHASE_EXPIRATION;
        }
    }

    /* Decay piriform activation */
    float decay = expf(-dt / 100.0f);
    for (uint32_t i = 0; i < olfact->num_piriform; i++) {
        olfact->piriform_activation[i] *= decay;
    }

    /* Update adaptation */
    olfact->adaptation_level *= expf(-dt / OLFACT_ADAPTATION_TAU);

    olfact->updates_processed++;
    olfact->last_update_time = olfact_get_time_ms();
    return 0;
}

int olfact_process_odor(nimcp_olfactory_t* olfact,
                        const float* receptor_pattern,
                        uint32_t pattern_size,
                        float concentration) {
    if (!olfact || !receptor_pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_update: required parameter is NULL (olfact, receptor_pattern)");
        return -1;
    }
    if (pattern_size > OLFACT_MAX_RECEPTORS) pattern_size = OLFACT_MAX_RECEPTORS;

    olfact->status = OLFACT_STATUS_PROCESSING;

    /* Apply adaptation */
    float effective_concentration = concentration * (1.0f - olfact->adaptation_level);

    /* Store current pattern */
    memcpy(olfact->current_pattern.receptor_activations, receptor_pattern,
           pattern_size * sizeof(float));
    olfact->current_pattern.num_active_receptors = 0;
    olfact->current_pattern.total_activation = 0.0f;

    for (uint32_t i = 0; i < pattern_size; i++) {
        float activation = receptor_pattern[i] * effective_concentration;
        olfact->current_pattern.receptor_activations[i] = activation;
        if (activation > OLFACT_DETECTION_THRESHOLD) {
            olfact->current_pattern.num_active_receptors++;
        }
        olfact->current_pattern.total_activation += activation;
    }
    olfact->current_pattern.timestamp = olfact_get_time_ms();

    /* Update glomeruli */
    for (uint32_t i = 0; i < olfact->num_glomeruli && i < pattern_size; i++) {
        olfact->glomeruli[i].activation = olfact->current_pattern.receptor_activations[i];
    }

    /* Update mitral cells (simplified) */
    for (uint32_t i = 0; i < olfact->num_mitral_cells; i++) {
        uint32_t glom_id = i % olfact->num_glomeruli;
        olfact->mitral_cells[i].activation = olfact->glomeruli[glom_id].activation;
        olfact->mitral_cells[i].output_to_piriform = olfact->mitral_cells[i].activation;
    }

    /* Update piriform cortex */
    for (uint32_t i = 0; i < olfact->num_piriform; i++) {
        float input = 0.0f;
        uint32_t start = (i * olfact->num_mitral_cells) / olfact->num_piriform;
        uint32_t end = ((i + 1) * olfact->num_mitral_cells) / olfact->num_piriform;
        for (uint32_t j = start; j < end; j++) {
            input += olfact->mitral_cells[j].output_to_piriform;
        }
        olfact->piriform_activation[i] += (end > start) ? input / (float)(end - start) : 0.0f;
        if (olfact->piriform_activation[i] > 1.0f) {
            olfact->piriform_activation[i] = 1.0f;
        }
    }

    /* Update adaptation */
    olfact->adaptation_level += 0.01f * effective_concentration;
    if (olfact->adaptation_level > 0.9f) {
        olfact->adaptation_level = 0.9f;
    }

    olfact->status = OLFACT_STATUS_READY;
    return 0;
}

int olfact_identify_odor(nimcp_olfactory_t* olfact, olfact_odor_id_t* result) {
    if (!olfact || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_identify_odor: required parameter is NULL (olfact, result)");
        return -1;
    }

    olfact->status = OLFACT_STATUS_IDENTIFYING;

    /* Simple identification: find best matching known odor */
    float best_match = 0.0f;
    int best_idx = -1;

    for (uint32_t i = 0; i < olfact->num_known_odors; i++) {
        if (olfact->known_odors[i].pattern) {
            float similarity = olfact_compare_odors(olfact,
                olfact->current_pattern.receptor_activations,
                olfact->known_odors[i].pattern,
                OLFACT_MAX_RECEPTORS);
            if (similarity > best_match) {
                best_match = similarity;
                best_idx = i;
            }
        }
    }

    if (best_idx >= 0 && best_match > 0.5f) {
        memcpy(result, &olfact->known_odors[best_idx], sizeof(olfact_odor_id_t));
        result->confidence = best_match;
    } else {
        result->odor_id = 0;
        strncpy(result->name, "Unknown", sizeof(result->name) - 1);
        result->name[sizeof(result->name) - 1] = '\0';
        result->category = ODOR_CAT_UNKNOWN;
        result->valence = HEDONIC_NEUTRAL;
        result->intensity = olfact->current_pattern.total_activation;
        result->confidence = 0.0f;
        result->familiarity = 0.0f;
    }

    memcpy(&olfact->current_odor, result, sizeof(olfact_odor_id_t));
    olfact->status = OLFACT_STATUS_READY;
    return 0;
}

odor_category_t olfact_classify_odor(nimcp_olfactory_t* olfact) {
    if (!olfact) return ODOR_CAT_UNKNOWN;
    return olfact->current_odor.category;
}

hedonic_valence_t olfact_get_valence(nimcp_olfactory_t* olfact) {
    if (!olfact) return HEDONIC_NEUTRAL;
    return olfact->current_odor.valence;
}

float olfact_get_intensity(nimcp_olfactory_t* olfact) {
    if (!olfact) return 0.0f;
    /* Normalize intensity to [0, 1] range */
    float intensity = olfact->current_pattern.total_activation;
    if (olfact->current_pattern.num_active_receptors > 0) {
        intensity /= olfact->current_pattern.num_active_receptors;
    }
    return intensity > 1.0f ? 1.0f : intensity;
}

float olfact_compare_odors(nimcp_olfactory_t* olfact,
                           const float* pattern1,
                           const float* pattern2,
                           uint32_t dim) {
    (void)olfact;  /* olfact not needed for comparison */
    if (!pattern1 || !pattern2 || dim == 0) return 0.0f;

    /* Cosine similarity */
    float dot = 0.0f, norm1 = 0.0f, norm2 = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        dot += pattern1[i] * pattern2[i];
        norm1 += pattern1[i] * pattern1[i];
        norm2 += pattern2[i] * pattern2[i];
    }

    if (norm1 < 1e-10f || norm2 < 1e-10f) return 0.0f;
    return dot / (sqrtf(norm1) * sqrtf(norm2));
}

int olfact_start_sniff(nimcp_olfactory_t* olfact, float strength) {
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_start_sniff: olfact is NULL");
        return -1;
    }
    olfact->sniff_state.sniff_strength = strength;
    olfact->sniff_state.cycle_position = 0.0f;
    olfact->sniff_state.phase = SNIFF_PHASE_INSPIRATION;
    olfact->sniff_state.cycle_start = olfact_get_time_ms();
    olfact->status = OLFACT_STATUS_SNIFFING;
    return 0;
}

sniff_phase_t olfact_get_sniff_phase(nimcp_olfactory_t* olfact) {
    if (!olfact) return SNIFF_PHASE_BASELINE;
    return olfact->sniff_state.phase;
}

float olfact_get_sniff_modulation(nimcp_olfactory_t* olfact) {
    if (!olfact) return 0.0f;
    /* Peak during inspiration phase */
    if (olfact->sniff_state.phase == SNIFF_PHASE_INSPIRATION ||
        olfact->sniff_state.phase == SNIFF_PHASE_PEAK) {
        return olfact->sniff_state.sniff_strength;
    }
    return 0.3f;
}

int olfact_store_memory(nimcp_olfactory_t* olfact,
                        const olfact_odor_id_t* odor,
                        float emotional_valence,
                        float emotional_arousal,
                        const char* context) {
    if (!olfact || !odor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_get_sniff_modulation: required parameter is NULL (olfact, odor)");
        return -1;
    }
    if (olfact->num_memories >= olfact->max_memories) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "olfact_get_sniff_modulation: capacity exceeded");
        return -1;
    }

    olfact_memory_t* mem = &olfact->odor_memories[olfact->num_memories];
    mem->memory_id = olfact->num_memories;
    memcpy(&mem->odor, odor, sizeof(olfact_odor_id_t));
    mem->emotional_valence = emotional_valence;
    mem->emotional_arousal = emotional_arousal;
    mem->memory_strength = 1.0f;
    mem->encoding_time = olfact_get_time_ms();
    mem->last_recall = mem->encoding_time;
    mem->recall_count = 0;

    if (context) {
        strncpy(mem->associated_context, context, sizeof(mem->associated_context) - 1);
    }

    olfact->num_memories++;
    return 0;
}

float olfact_get_adaptation_level(nimcp_olfactory_t* olfact) {
    if (!olfact) return 0.0f;
    return olfact->adaptation_level;
}

int olfact_reset_adaptation(nimcp_olfactory_t* olfact) {
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_reset_adaptation: olfact is NULL");
        return -1;
    }
    olfact->adaptation_level = 0.0f;
    return 0;
}

/* Bridge initialization */
int olfact_init_prime_resonance_bridge(nimcp_olfactory_t* olfact, void* pr_memory) {
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_init_prime_resonance_bridge: olfact is NULL");
        return -1;
    }
    olfact->prime_resonance_bridge.pr_memory_ctx = pr_memory;
    olfact->prime_resonance_bridge.initialized = true;
    return 0;
}

int olfact_init_immune_bridge(nimcp_olfactory_t* olfact, void* immune) {
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_init_immune_bridge: olfact is NULL");
        return -1;
    }
    olfact->immune_bridge.immune_system = immune;
    olfact->immune_bridge.initialized = true;
    olfact->immune_bridge.health_score = 1.0f;
    return 0;
}

int olfact_init_amygdala_bridge(nimcp_olfactory_t* olfact, void* amygdala) {
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_init_amygdala_bridge: olfact is NULL");
        return -1;
    }
    olfact->amygdala_bridge.amygdala = amygdala;
    olfact->amygdala_bridge.initialized = true;
    return 0;
}

int olfact_init_entorhinal_bridge(nimcp_olfactory_t* olfact, void* entorhinal) {
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_init_entorhinal_bridge: olfact is NULL");
        return -1;
    }
    olfact->entorhinal_bridge.entorhinal = entorhinal;
    olfact->entorhinal_bridge.initialized = true;
    return 0;
}

int olfact_init_ofc_bridge(nimcp_olfactory_t* olfact, void* ofc) {
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_init_ofc_bridge: olfact is NULL");
        return -1;
    }
    olfact->ofc_bridge.orbitofrontal = ofc;
    olfact->ofc_bridge.initialized = true;
    return 0;
}

int olfact_init_hypothalamus_bridge(nimcp_olfactory_t* olfact, void* hypothalamus) {
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_init_hypothalamus_bridge: olfact is NULL");
        return -1;
    }
    olfact->hypothalamus_bridge.hypothalamus = hypothalamus;
    olfact->hypothalamus_bridge.initialized = true;
    return 0;
}

int olfact_init_logging_bridge(nimcp_olfactory_t* olfact, void* logger) {
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_init_logging_bridge: olfact is NULL");
        return -1;
    }
    olfact->logging_bridge.logger = logger;
    olfact->logging_bridge.initialized = true;
    strncpy(olfact->logging_bridge.log_prefix, "OLFACT", sizeof(olfact->logging_bridge.log_prefix) - 1);
    return 0;
}

int olfact_init_snn_bridge(nimcp_olfactory_t* olfact, void* snn) {
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_init_snn_bridge: olfact is NULL");
        return -1;
    }
    olfact->snn_bridge.snn = snn;
    olfact->snn_bridge.neuron_ids = NULL;
    olfact->snn_bridge.num_mapped_neurons = 0;
    olfact->snn_bridge.snn_activation_gain = 1.0f;
    olfact->snn_bridge.initialized = (snn != NULL);
    return 0;
}

int olfact_init_plasticity_bridge(nimcp_olfactory_t* olfact, void* plasticity, void* stdp) {
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_init_plasticity_bridge: olfact is NULL");
        return -1;
    }
    olfact->plasticity_bridge.plasticity_coordinator = plasticity;
    olfact->plasticity_bridge.stdp_context = stdp;
    olfact->plasticity_bridge.learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    olfact->plasticity_bridge.olfactory_plasticity_gate = 1.0f;
    olfact->plasticity_bridge.hebbian_enabled = true;
    olfact->plasticity_bridge.initialized = (plasticity != NULL || stdp != NULL);
    return 0;
}

int olfact_init_bio_async_bridge(nimcp_olfactory_t* olfact, void* runtime) {
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_init_bio_async_bridge: olfact is NULL");
        return -1;
    }
    olfact->bio_async_embed.runtime = runtime;
    olfact->bio_async_embed.subscription_mask = 0xFFFFFFFF; /* Subscribe to all */
    olfact->bio_async_embed.messages_sent = 0;
    olfact->bio_async_embed.messages_received = 0;
    olfact->bio_async_embed.initialized = (runtime != NULL);
    return 0;
}

/* Bidirectional flow */
int olfact_process_incoming(nimcp_olfactory_t* olfact) {
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_process_incoming: olfact is NULL");
        return -1;
    }
    return 0;
}

int olfact_send_outgoing(nimcp_olfactory_t* olfact) {
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_send_outgoing: olfact is NULL");
        return -1;
    }
    return 0;
}

int olfact_bidirectional_update(nimcp_olfactory_t* olfact, float dt) {
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_bidirectional_update: olfact is NULL");
        return -1;
    }
    olfact_process_incoming(olfact);
    olfact_update(olfact, dt);
    olfact_send_outgoing(olfact);
    return 0;
}

int olfact_sync_amygdala(nimcp_olfactory_t* olfact) {
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_sync_amygdala: olfact is NULL");
        return -1;
    }
    if (!olfact->amygdala_bridge.initialized) return 0;  /* No-op if not initialized */
    /* Sync with amygdala for emotional associations */
    return 0;
}

int olfact_sync_entorhinal(nimcp_olfactory_t* olfact) {
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_sync_entorhinal: olfact is NULL");
        return -1;
    }
    if (!olfact->entorhinal_bridge.initialized) return 0;  /* No-op if not initialized */
    /* Sync with entorhinal cortex for memory encoding */
    return 0;
}

int olfact_sync_ofc(nimcp_olfactory_t* olfact) {
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_sync_ofc: olfact is NULL");
        return -1;
    }
    if (!olfact->ofc_bridge.initialized) return 0;  /* No-op if not initialized */
    /* Sync with orbitofrontal cortex for valuation */
    return 0;
}

/* Status and diagnostics */
olfact_status_t olfact_get_status(nimcp_olfactory_t* olfact) {
    if (!olfact) return OLFACT_STATUS_ERROR;
    return olfact->status;
}

olfact_error_t olfact_get_last_error(nimcp_olfactory_t* olfact) {
    if (!olfact) return OLFACT_ERROR_INTERNAL;
    return olfact->last_error;
}

const char* olfact_error_string(olfact_error_t error) {
    switch (error) {
        case OLFACT_ERROR_NONE: return "No error";
        case OLFACT_ERROR_INVALID_INPUT: return "Invalid input";
        case OLFACT_ERROR_SATURATION: return "Saturation";
        case OLFACT_ERROR_IDENTIFICATION_FAILED: return "Identification failed";
        case OLFACT_ERROR_MEMORY_FULL: return "Memory full";
        case OLFACT_ERROR_BRIDGE_ERROR: return "Bridge error";
        case OLFACT_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* olfact_status_string(olfact_status_t status) {
    switch (status) {
        case OLFACT_STATUS_IDLE: return "Idle";
        case OLFACT_STATUS_READY: return "Ready";
        case OLFACT_STATUS_SNIFFING: return "Sniffing";
        case OLFACT_STATUS_PROCESSING: return "Processing";
        case OLFACT_STATUS_IDENTIFYING: return "Identifying";
        case OLFACT_STATUS_ADAPTING: return "Adapting";
        case OLFACT_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

int olfact_get_stats(nimcp_olfactory_t* olfact, olfact_stats_t* stats) {
    if (!olfact || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_get_stats: required parameter is NULL (olfact, stats)");
        return -1;
    }
    stats->odors_detected = olfact->updates_processed;
    stats->odors_identified = olfact->num_known_odors;
    stats->memories_stored = olfact->num_memories;
    stats->sniff_cycles = olfact->sniff_state.cycle_count;
    stats->current_adaptation_level = olfact->adaptation_level;
    stats->last_update_time = olfact->last_update_time;
    return 0;
}

float olfact_get_health_status(nimcp_olfactory_t* olfact) {
    if (!olfact) return 0.0f;
    if (olfact->status == OLFACT_STATUS_ERROR) return 0.0f;
    float health = 1.0f;
    if (olfact->immune_bridge.initialized) {
        health *= olfact->immune_bridge.health_score;
        if (olfact->immune_bridge.nasal_congestion) health *= 0.5f;
    }
    return health;
}

/* Utility functions */
const char* olfact_category_name(odor_category_t category) {
    static const char* names[] = {
        "Unknown", "Floral", "Fruity", "Citrus", "Woody", "Minty",
        "Sweet", "Spicy", "Savory", "Smoky", "Chemical", "Decayed", "Pungent"
    };
    if (category >= ODOR_CAT_COUNT) return "Unknown";
    return names[category];
}

const char* olfact_valence_name(hedonic_valence_t valence) {
    static const char* names[] = {
        "Very Unpleasant", "Unpleasant", "Slightly Unpleasant",
        "Neutral", "Slightly Pleasant", "Pleasant", "Very Pleasant"
    };
    if (valence > HEDONIC_VERY_PLEASANT) return "Unknown";
    return names[valence];
}

const char* olfact_sniff_phase_name(sniff_phase_t phase) {
    switch (phase) {
        case SNIFF_PHASE_BASELINE: return "Baseline";
        case SNIFF_PHASE_INSPIRATION: return "Inspiration";
        case SNIFF_PHASE_PEAK: return "Peak";
        case SNIFF_PHASE_EXPIRATION: return "Expiration";
        default: return "Unknown";
    }
}

/* Serialization */
size_t olfact_get_serialization_size(nimcp_olfactory_t* olfact) {
    if (!olfact) return 0;
    /* config + num_memories + memories array */
    return sizeof(olfact_config_t) + sizeof(uint32_t) +
           olfact->num_memories * sizeof(olfact_memory_t);
}

int olfact_serialize(nimcp_olfactory_t* olfact, uint8_t* buffer, size_t size, size_t* written) {
    if (!olfact || !buffer || !written) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_serialize: required parameter is NULL (olfact, buffer, written)");
        return -1;
    }
    size_t needed = olfact_get_serialization_size(olfact);
    if (size < needed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "olfact_serialize: validation failed");
        return -1;
    }

    size_t offset = 0;
    memcpy(buffer + offset, &olfact->config, sizeof(olfact_config_t));
    offset += sizeof(olfact_config_t);

    memcpy(buffer + offset, &olfact->num_memories, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    if (olfact->num_memories > 0) {
        memcpy(buffer + offset, olfact->odor_memories,
               olfact->num_memories * sizeof(olfact_memory_t));
        offset += olfact->num_memories * sizeof(olfact_memory_t);
    }

    *written = offset;
    return 0;
}

nimcp_olfactory_t* olfact_deserialize(const uint8_t* buffer, size_t size, size_t* bytes_read) {
    if (!buffer || !bytes_read) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "olfact_deserialize: required parameter is NULL (buffer, bytes_read)");
        return NULL;
    }
    if (size < sizeof(olfact_config_t) + sizeof(uint32_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "olfact_deserialize: validation failed");
        return NULL;
    }

    size_t offset = 0;
    olfact_config_t config;
    memcpy(&config, buffer + offset, sizeof(olfact_config_t));
    offset += sizeof(olfact_config_t);

    uint32_t num_memories;
    memcpy(&num_memories, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    nimcp_olfactory_t* olfact = olfact_create(&config);
    if (!olfact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "olfact_deserialize: allocation failed");
        return NULL;
    }

    if (num_memories > 0 && num_memories <= olfact->max_memories) {
        memcpy(olfact->odor_memories, buffer + offset,
               num_memories * sizeof(olfact_memory_t));
        olfact->num_memories = num_memories;
        offset += num_memories * sizeof(olfact_memory_t);
    }

    *bytes_read = offset;
    return olfact;
}
