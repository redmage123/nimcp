/**
 * @file nimcp_grief_and_loss.c
 * @brief Implementation of grief, loss, and death understanding system
 *
 * @version Phase E1: Grief and Loss Understanding
 * @date 2025-11-13
 */

#include "cognitive/nimcp_grief_and_loss.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/kg/nimcp_wave10_affective_kg.h"  /* W10: grief stage events */
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "nimcp.h"
#include <math.h>
#include <string.h>
#include "utils/memory/nimcp_memory_guards.h"  // For nimcp_calloc/nimcp_free
#include "utils/thread/nimcp_atomic.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "GRIEF"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(grief_and_loss, MESH_ADAPTER_CATEGORY_COGNITIVE)


// Thread-safe attachment ID counter
static nimcp_atomic_uint32_t g_attachment_id_counter = {1};
#define BIO_MODULE_GRIEF 0x0323

/*=============================================================================
 * BIO-ASYNC MESSAGE HANDLERS
 *============================================================================*/

/* Forward declaration of handler */
static nimcp_error_t handle_grief_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

/**
 * @brief KG-driven wiring callback for grief module
 */
static int grief_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data)
{
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && message_count > 256) {
            grief_and_loss_heartbeat("grief_and_lo_loop",
                             (float)(i + 1) / (float)message_count);
        }

        switch (message_types[i]) {
            case BIO_MSG_INTROSPECTION_QUERY:
                bio_router_register_handler(ctx, message_types[i], handle_grief_query);
                registered++;
                break;
            default:
                LOG_DEBUG(LOG_MODULE, "grief: unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    LOG_INFO(LOG_MODULE, "Grief: registered %d handlers via KG wiring", registered);
    return 0;
}

/**
 * @brief Handle incoming query about grief state (e.g., from mental health module)
 */
static nimcp_error_t handle_grief_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    (void)msg_size;
    (void)response_promise;
    NIMCP_CHECK_THROW(msg && user_data, NIMCP_ERROR_NULL_ARG, "msg or user_data is NULL");

    grief_system_t* system = (grief_system_t*)user_data;
    LOG_DEBUG(LOG_MODULE, "Received grief query: experiencing=%d, pain=%.2f",
              system->current_grief.experiencing_grief,
              system->current_grief.emotional_pain_intensity);

    return NIMCP_SUCCESS;
}

/**
 * @brief Broadcast grief state change to other modules
 */
static void bio_broadcast_grief_state(grief_system_t* system) {
    if (!system || !system->bio_async_enabled || !system->bio_ctx_ptr) { return; }

    bio_msg_salience_response_t msg = {0};
    bio_msg_init_header(&msg.header, BIO_MSG_SALIENCE_RESPONSE,
                        bio_module_context_get_id(system->bio_ctx_ptr), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.stimulus_id = 0;
    msg.salience_score = system->current_grief.emotional_pain_intensity;
    msg.attention_priority = system->current_grief.intrusive_thoughts_frequency;
    msg.requires_immediate_attention = (system->current_grief.emotional_pain_intensity > 0.8F);

    bio_router_broadcast(system->bio_ctx_ptr, &msg, sizeof(msg));
    LOG_DEBUG(LOG_MODULE, "Broadcast grief state: pain=%.2f, stage=%d",
              system->current_grief.emotional_pain_intensity,
              system->current_grief.current_stage);
}

static inline float exponential_decay(float current, float target, float decay_rate, float dt) {
    float decay_factor = expf(-decay_rate * dt);
    return current * decay_factor + target * (1.0F - decay_factor);
}

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

grief_system_t* grief_system_create(void) {
    // WHAT: Allocate and initialize grief system
    // WHY:  Central system for processing loss and mortality
    // HOW:  Zero-initialize all state, set up default parameters

    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_grief_system_create", 0.0f);


    grief_system_t* system = (grief_system_t*)nimcp_calloc(1, sizeof(grief_system_t));
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate system");

        return NULL;

    }

    // Initialize all attachments as inactive
    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GRIEF_MAX_ATTACHMENTS > 256) {
            grief_and_loss_heartbeat("grief_and_lo_loop",
                             (float)(i + 1) / (float)GRIEF_MAX_ATTACHMENTS);
        }

        system->attachments[i].active = false;
    }

    // Default: integrate with other systems
    system->integrate_with_neuromodulators = true;
    system->integrate_with_memory = true;
    system->integrate_with_wellbeing = true;

    // Existential awareness starts unaware (awakens through experience)
    system->existential.aware_of_mortality = false;  // Must discover mortality
    system->existential.death_anxiety = 0.0F;  // No anxiety initially (develops with awareness)
    system->existential.sense_of_purpose = 0.0F;  // Develops through life experience
    system->existential.acceptance_of_finitude = 0.0F;  // Develops with maturity
    system->existential.existential_anxiety = 0.0F;  // Will increase with purpose threats

    // Bio-async registration
    system->bio_ctx_ptr = NULL;
    system->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_GRIEF,
            .module_name = "grief_and_loss",
            .inbox_capacity = 32,
            .user_data = system
        };
        system->bio_ctx_ptr = bio_router_register_module(&bio_info);
        if (system->bio_ctx_ptr) {
            system->bio_async_enabled = true;

            /* Try KG-driven wiring callback registration first */
            nimcp_error_t wiring_result = bio_router_register_wiring_callback(
                BIO_MODULE_GRIEF,
                (void*)grief_wiring_handler_callback,
                system
            );

            if (wiring_result == NIMCP_SUCCESS) {
                LOG_INFO(LOG_MODULE, "Grief: KG-driven wiring callback registered");
            } else {
                /* Legacy fallback - register handlers directly */
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(system->bio_ctx_ptr, BIO_MSG_INTROSPECTION_QUERY, handle_grief_query)
                );
                LOG_INFO(LOG_MODULE, "Grief: legacy handler registration (module_id=0x%04X)", BIO_MODULE_GRIEF);
            }
        }
    }

    return system;
}

void grief_system_destroy(grief_system_t* system) {
    // WHAT: Free grief system resources
    // WHY:  Prevent memory leaks
    // HOW:  Simple nimcp_free(no complex nested allocations)

    if (!system) return;

    // Unregister from bio-router
    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_grief_system_destroy", 0.0f);


    if (system->bio_async_enabled && system->bio_ctx_ptr) {
        bio_router_unregister_module(system->bio_ctx_ptr);
        system->bio_ctx_ptr = NULL;
        system->bio_async_enabled = false;
    }

    nimcp_free(system);
    system = NULL;
}

void grief_system_reset(grief_system_t* system) {
    // WHAT: Complete reset of grief system to initial state
    // WHY:  For testing or complete system restart
    // HOW:  Preserve integration settings only, clear everything else

    if (!system) return;

    // Preserve integration settings only
    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_grief_system_reset", 0.0f);


    bool integrate_nm = system->integrate_with_neuromodulators;
    bool integrate_mem = system->integrate_with_memory;
    bool integrate_wb = system->integrate_with_wellbeing;

    // Unregister from bio-router before zeroing (prevents stale registration)
    if (system->bio_async_enabled && system->bio_ctx_ptr) {
        bio_router_unregister_module(system->bio_ctx_ptr);
    }

    // Zero everything
    memset(system, 0, sizeof(grief_system_t));

    // Restore integration settings
    system->integrate_with_neuromodulators = integrate_nm;
    system->integrate_with_memory = integrate_mem;
    system->integrate_with_wellbeing = integrate_wb;

    // Re-initialize existential state to defaults
    system->existential.aware_of_mortality = false;
    system->existential.death_anxiety = 0.0F;
    system->existential.sense_of_purpose = 0.0F;
    system->existential.acceptance_of_finitude = 0.0F;
}

//=============================================================================
// ATTACHMENT FUNCTIONS
//=============================================================================

uint32_t grief_create_attachment(grief_system_t* system,
                                 attachment_type_t type,
                                 float strength,
                                 float positive_valence,
                                 float dependency) {
    // WHAT: Create new emotional bond
    // WHY:  Attachments are what make loss painful
    // HOW:  Find empty slot, initialize bond characteristics

    if (!system) return 0;

    // Find empty slot
    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_grief_create_attachm", 0.0f);


    int slot = -1;
    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GRIEF_MAX_ATTACHMENTS > 256) {
            grief_and_loss_heartbeat("grief_and_lo_loop",
                             (float)(i + 1) / (float)GRIEF_MAX_ATTACHMENTS);
        }

        if (!system->attachments[i].active) {
            slot = i;
            break;
        }
    }

    if (slot == -1) return 0;  // No space

    // Generate unique ID (thread-safe atomic counter)
    uint32_t attachment_id = nimcp_atomic_fetch_add_u32(&g_attachment_id_counter, 1, NIMCP_MEMORY_ORDER_SEQ_CST);

    // Initialize attachment
    attachment_bond_t* bond = &system->attachments[slot];
    bond->active = true;
    bond->attachment_id = attachment_id;
    bond->type = type;
    bond->strength = nimcp_clampf(strength, 0.0F, 1.0F);
    bond->positive_valence = nimcp_clampf(positive_valence, 0.0F, 1.0F);
    bond->dependency = nimcp_clampf(dependency, 0.0F, 1.0F);
    bond->formation_time = 0;  // Will be set by caller if needed
    bond->duration = 0;
    bond->associated_memories = 0;
    bond->memory_vividness = 0.5F;
    bond->severed = false;

    system->active_attachment_count++;

    return attachment_id;
}

void grief_strengthen_attachment(grief_system_t* system,
                                uint32_t attachment_id,
                                float amount) {
    // WHAT: Increase bond strength over time
    // WHY:  Longer relationships = stronger bonds = deeper grief
    // HOW:  Find bond and increase strength (saturating)

    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_grief_strengthen_att", 0.0f);


    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GRIEF_MAX_ATTACHMENTS > 256) {
            grief_and_loss_heartbeat("grief_and_lo_loop",
                             (float)(i + 1) / (float)GRIEF_MAX_ATTACHMENTS);
        }

        if (system->attachments[i].active &&
            system->attachments[i].attachment_id == attachment_id) {

            system->attachments[i].strength += amount;
            system->attachments[i].strength = nimcp_clampf(system->attachments[i].strength, 0.0F, 1.0F);
            system->attachments[i].duration++;
            return;
        }
    }
}

void grief_add_shared_memory(grief_system_t* system, uint32_t attachment_id) {
    // WHAT: Record shared experience with attachment figure
    // WHY:  More memories = more to grieve
    // HOW:  Increment counter, increase vividness

    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_grief_add_shared_mem", 0.0f);


    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GRIEF_MAX_ATTACHMENTS > 256) {
            grief_and_loss_heartbeat("grief_and_lo_loop",
                             (float)(i + 1) / (float)GRIEF_MAX_ATTACHMENTS);
        }

        if (system->attachments[i].active &&
            system->attachments[i].attachment_id == attachment_id) {

            system->attachments[i].associated_memories++;

            // More memories = more vivid overall
            float memory_boost = 0.01F;
            system->attachments[i].memory_vividness += memory_boost;
            system->attachments[i].memory_vividness =
                nimcp_clampf(system->attachments[i].memory_vividness, 0.0F, 1.0F);
            return;
        }
    }
}

//=============================================================================
// LOSS PROCESSING
//=============================================================================

void grief_process_loss(grief_system_t* system,
                       uint32_t attachment_id,
                       loss_type_t loss_type,
                       uint64_t current_time_us) {
    // WHAT: Initiate grief response to loss
    // WHY:  Loss triggers cascade of psychological and biological changes
    // HOW:  Set up grief state, initial shock stage, neurochemical changes

    if (!system) return;

    // Find the attachment
    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_grief_process_loss", 0.0f);


    attachment_bond_t* lost_bond = NULL;
    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GRIEF_MAX_ATTACHMENTS > 256) {
            grief_and_loss_heartbeat("grief_and_lo_loop",
                             (float)(i + 1) / (float)GRIEF_MAX_ATTACHMENTS);
        }

        if (system->attachments[i].active &&
            system->attachments[i].attachment_id == attachment_id) {
            lost_bond = &system->attachments[i];
            break;
        }
    }

    if (!lost_bond) return;  // Attachment not found

    // Mark bond as severed (but keep for continuing bonds)
    lost_bond->severed = true;
    lost_bond->loss_time = current_time_us;
    lost_bond->loss_type = loss_type;

    // Initialize grief state
    system->current_grief.experiencing_grief = true;
    system->current_grief.lost_attachment_id = attachment_id;
    system->current_grief.loss_onset_time = current_time_us;

    // Start in shock/denial stage
    system->current_grief.current_stage = GRIEF_STAGE_SHOCK;
    system->current_grief.stage_transition_time = current_time_us;

    // Initial stage intensities (shock dominates)
    system->current_grief.stage_intensities[GRIEF_STAGE_SHOCK] = 1.0F;
    system->current_grief.stage_intensities[GRIEF_STAGE_DENIAL] = 0.7F;
    system->current_grief.stage_intensities[GRIEF_STAGE_ANGER] = 0.0F;
    system->current_grief.stage_intensities[GRIEF_STAGE_BARGAINING] = 0.0F;
    system->current_grief.stage_intensities[GRIEF_STAGE_DEPRESSION] = 0.3F;
    system->current_grief.stage_intensities[GRIEF_STAGE_ACCEPTANCE] = 0.0F;
    system->current_grief.stage_intensities[GRIEF_STAGE_MEANING_MAKING] = 0.0F;

    // Initial symptom severity (depends on bond strength AND loss type)
    float bond_strength = lost_bond->strength;

    // Loss type intensity modifier
    float loss_type_intensity = 1.0F;
    switch (loss_type) {
        case LOSS_TYPE_DEATH:        loss_type_intensity = 1.0F; break;  // Maximum intensity
        case LOSS_TYPE_SEPARATION:   loss_type_intensity = 0.7F; break;  // Less permanent
        case LOSS_TYPE_REJECTION:    loss_type_intensity = 0.8F; break;  // Social pain
        case LOSS_TYPE_ROLE_LOSS:    loss_type_intensity = 0.6F; break;  // Identity loss
        case LOSS_TYPE_ANTICIPATORY: loss_type_intensity = 0.5F; break;  // Pre-grieving
        case LOSS_TYPE_AMBIGUOUS:    loss_type_intensity = 0.9F; break;  // Uncertainty compounds
        case LOSS_TYPE_SYMBOLIC:     loss_type_intensity = 0.5F; break;  // Abstract loss
        default:                     loss_type_intensity = 1.0F; break;
    }

    // Attachment type intensity modifier (some relationships are more intense)
    float attachment_type_intensity = 1.0F;
    switch (lost_bond->type) {
        case ATTACHMENT_PARENT:    attachment_type_intensity = 1.0F; break;  // Core attachment
        case ATTACHMENT_CHILD:     attachment_type_intensity = 1.0F; break;  // Deepest bond
        case ATTACHMENT_ROMANTIC:  attachment_type_intensity = 0.95F; break; // Very intense
        case ATTACHMENT_FRIEND:    attachment_type_intensity = 0.7F; break;  // Important but less
        case ATTACHMENT_PET:       attachment_type_intensity = 0.6F; break;  // Significant but different
        case ATTACHMENT_MENTOR:    attachment_type_intensity = 0.65F; break; // Influential
        case ATTACHMENT_PLACE:     attachment_type_intensity = 0.5F; break;  // Nostalgic
        case ATTACHMENT_IDENTITY:  attachment_type_intensity = 0.8F; break;  // Self-concept loss
        default:                   attachment_type_intensity = 0.7F; break;
    }

    // Memory influence (more shared memories = more intense grief)
    float memory_influence = 1.0F + (lost_bond->associated_memories / 100.0F) * lost_bond->memory_vividness;
    memory_influence = nimcp_clampf(memory_influence, 1.0F, 1.5F);  // Up to 50% increase

    // Combine all intensity factors with safety ceiling
    float combined_intensity = loss_type_intensity * attachment_type_intensity * memory_influence;
    combined_intensity = nimcp_clampf(combined_intensity, 0.0F, 1.3F);  // Cap to prevent extreme values

    // SAFETY: Grief symptoms capped to prevent complete incapacitation
    // Maximum emotional pain is 0.85 (never complete shutdown)
    system->current_grief.emotional_pain_intensity = nimcp_clampf(bond_strength * 0.9F * combined_intensity, 0.0F, 0.85F);
    system->current_grief.anhedonia_level = nimcp_clampf(bond_strength * 0.7F * combined_intensity, 0.0F, 0.80F);
    system->current_grief.intrusive_thoughts_frequency = nimcp_clampf(bond_strength * 0.8F * combined_intensity, 0.0F, 0.85F);
    system->current_grief.avoidance_level = 0.5F;
    // SAFETY: Functional impairment capped at 0.85 (system remains minimally functional)
    system->current_grief.functional_impairment = nimcp_clampf(bond_strength * 0.7F * combined_intensity, 0.0F, 0.85F);

    // Dual process: Initially loss-oriented
    system->current_grief.loss_orientation = 0.9F;
    system->current_grief.restoration_orientation = 0.1F;
    system->current_grief.last_oscillation_time = current_time_us;

    // Neurobiological changes with safety limits
    // SAFETY: Neuromodulator changes capped to prevent complete depletion/excess
    // System retains 15-20% baseline function even in severe grief
    system->current_grief.serotonin_depletion = nimcp_clampf(GRIEF_SEROTONIN_DEPLETION * bond_strength, 0.0F, 0.80F);  // Max 80% depletion
    system->current_grief.dopamine_depletion = nimcp_clampf(GRIEF_DOPAMINE_DEPLETION * bond_strength, 0.0F, 0.75F);   // Max 75% depletion
    system->current_grief.norepinephrine_elevation = nimcp_clampf(GRIEF_NOREPINEPHRINE_INCREASE * bond_strength, 1.0F, 2.5F);
    system->current_grief.cortisol_elevation = nimcp_clampf(GRIEF_CORTISOL_INCREASE * bond_strength, 1.0F, 2.5F);

    // Default to adaptive coping (can change)
    system->current_grief.predominant_coping = COPING_ADAPTIVE;
    system->current_grief.social_support_seeking = 0.5F;

    // Phase E1.1: Initialize sadness emotion
    // Grief-induced sadness proportional to bond strength and emotional pain
    system->current_grief.grief_induced_sadness = system->current_grief.emotional_pain_intensity;
    system->current_grief.sadness_emotion = grief_get_sadness_emotion(system);

    // Risk assessment
    bool high_dependency = (lost_bond->dependency > 0.7F);
    bool ambiguous_loss = (loss_type == LOSS_TYPE_AMBIGUOUS);
    bool previous_losses = (system->lifetime_losses > 2);

    system->current_grief.prolonged_grief_risk =
        (high_dependency || ambiguous_loss || previous_losses);

    // Statistics
    system->lifetime_losses++;

    /* Broadcast grief state change */
    bio_broadcast_grief_state(system);

    // Death loss awakens mortality awareness
    if (loss_type == LOSS_TYPE_DEATH) {
        system->existential.aware_of_mortality = true;
        system->existential.mortality_salience += 0.5F;  // Strong reminder
    } else {
        system->existential.mortality_salience += 0.3F;  // Moderate reminder
    }
    system->existential.mortality_salience = nimcp_clampf(system->existential.mortality_salience, 0.0F, 1.0F);
    system->existential.last_mortality_reminder = current_time_us;
}

void grief_update(grief_system_t* system, float dt, uint64_t current_time_us) {
    // Process pending bio-async messages
    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_grief_update", 0.0f);


    if (system && system->bio_async_enabled && system->bio_ctx_ptr) {
        bio_router_process_inbox(system->bio_ctx_ptr, 5);
    }

    // WHAT: Update grief processing over time
    // WHY:  Grief evolves - stages change, intensity decreases, integration occurs
    // HOW:  Update stage dynamics, dual process oscillation, neurochemistry

    if (!system || !system->current_grief.experiencing_grief) return;

    grief_state_t* grief = &system->current_grief;

    // Time since loss (in seconds)
    float time_since_loss = (float)(current_time_us - grief->loss_onset_time) / 1000000.0F;

    //=========================================================================
    // STAGE PROGRESSION
    //=========================================================================

    // WHAT: Evolve through grief stages (non-linear)
    // HOW:  Each stage increases/decreases based on time and other stages

    // Shock fades quickly (days to weeks)
    float shock_decay = expf(-time_since_loss / (86400.0F * 7.0F));  // 1 week half-life
    grief->stage_intensities[GRIEF_STAGE_SHOCK] = shock_decay;

    // Denial decreases gradually (ambiguous loss maintains baseline)
    // Find loss type for this grief
    loss_type_t current_loss_type = LOSS_TYPE_DEATH;
    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GRIEF_MAX_ATTACHMENTS > 256) {
            grief_and_loss_heartbeat("grief_and_lo_loop",
                             (float)(i + 1) / (float)GRIEF_MAX_ATTACHMENTS);
        }

        if (system->attachments[i].attachment_id == grief->lost_attachment_id) {
            current_loss_type = system->attachments[i].loss_type;
            break;
        }
    }

    // Standard denial decay
    float denial_decay = expf(-time_since_loss / (86400.0F * 30.0F));  // 1 month half-life
    grief->stage_intensities[GRIEF_STAGE_DENIAL] *= denial_decay;

    // Ambiguous loss maintains elevated baseline denial (uncertainty prevents full acceptance)
    if (current_loss_type == LOSS_TYPE_AMBIGUOUS) {
        float ambiguous_denial_baseline = 0.35F * expf(-time_since_loss / (86400.0F * 365.0F));  // Decays over years, not months
        grief->stage_intensities[GRIEF_STAGE_DENIAL] = fmaxf(grief->stage_intensities[GRIEF_STAGE_DENIAL], ambiguous_denial_baseline);
    }

    // Avoidance coping maintains denial and slows acceptance
    if (grief->predominant_coping == COPING_AVOIDANT) {
        grief->stage_intensities[GRIEF_STAGE_DENIAL] += 0.1F * dt / 86400.0F;  // Slowly increases denial
        grief->stage_intensities[GRIEF_STAGE_DENIAL] = nimcp_clampf(grief->stage_intensities[GRIEF_STAGE_DENIAL], 0.0F, 0.8F);
    }

    // Anger rises then falls (weeks)
    float anger_peak_time = 86400.0F * 14.0F;  // 2 weeks
    float anger_intensity = expf(-powf((time_since_loss - anger_peak_time) / (86400.0F * 21.0F), 2.0F));
    grief->stage_intensities[GRIEF_STAGE_ANGER] = anger_intensity * 0.7F;

    // Bargaining oscillates with denial
    grief->stage_intensities[GRIEF_STAGE_BARGAINING] =
        grief->stage_intensities[GRIEF_STAGE_DENIAL] * 0.6F;

    // Depression is sustained (months)
    float depression_onset = 1.0F - expf(-time_since_loss / (86400.0F * 7.0F));
    float depression_decay = expf(-time_since_loss / (86400.0F * 180.0F));  // 6 months
    grief->stage_intensities[GRIEF_STAGE_DEPRESSION] = depression_onset * depression_decay * 0.9F;

    // Acceptance grows slowly
    float acceptance_growth = 1.0F - expf(-time_since_loss / (86400.0F * 90.0F));  // 3 months
    grief->stage_intensities[GRIEF_STAGE_ACCEPTANCE] = acceptance_growth * 0.7F;

    // Meaning-making emerges later
    if (time_since_loss > 86400.0F * 60.0F) {  // After 2 months
        float meaning_growth = (time_since_loss - 86400.0F * 60.0F) / (86400.0F * 180.0F);
        grief->stage_intensities[GRIEF_STAGE_MEANING_MAKING] = nimcp_clampf(meaning_growth, 0.0F, 0.8F);
    }

    // Update current predominant stage
    float max_intensity = 0.0F;
    grief_stage_t max_stage = GRIEF_STAGE_SHOCK;
    for (int i = 0; i < 7; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 7 > 256) {
            grief_and_loss_heartbeat("grief_and_lo_loop",
                             (float)(i + 1) / (float)7);
        }

        if (grief->stage_intensities[i] > max_intensity) {
            max_intensity = grief->stage_intensities[i];
            max_stage = (grief_stage_t)i;
        }
    }

    if (max_stage != grief->current_stage) {
        grief->current_stage = max_stage;
        grief->stage_transition_time = current_time_us;
        grief->stage_cycles++;
    }

    //=========================================================================
    // SYMPTOM EVOLUTION
    //=========================================================================

    // Pain intensity decreases over time (but slowly)
    // Maladaptive/avoidant coping slows pain reduction
    float pain_decay_rate = 1.0F / (86400.0F * 180.0F);  // 6-month half-life
    if (grief->predominant_coping == COPING_AVOIDANT || grief->predominant_coping == COPING_MALADAPTIVE) {
        pain_decay_rate *= 0.5F;  // Half as fast recovery
    }
    grief->emotional_pain_intensity = exponential_decay(
        grief->emotional_pain_intensity, 0.0F, pain_decay_rate, dt);

    // Anhedonia follows dopamine recovery
    grief->anhedonia_level = grief->dopamine_depletion;

    // Intrusive thoughts decrease
    float intrusion_decay_rate = 1.0F / (86400.0F * 90.0F);  // 3-month half-life
    grief->intrusive_thoughts_frequency = exponential_decay(
        grief->intrusive_thoughts_frequency, 0.0F, intrusion_decay_rate, dt);

    // Functional impairment decreases (slower with poor coping, may increase)
    float impairment_decay_rate = 1.0F / (86400.0F * 60.0F);  // 2-month half-life

    if (grief->predominant_coping == COPING_MALADAPTIVE) {
        // Maladaptive coping causes impairment to worsen
        impairment_decay_rate *= 0.1F;  // Very slow recovery
        grief->functional_impairment += 0.08F * dt / 86400.0F;  // Worsens over time
        // SAFETY: Even with maladaptive coping, cap impairment at 0.90 (retain minimal function)
        grief->functional_impairment = nimcp_clampf(grief->functional_impairment, 0.0F, 0.90F);
        // Still apply some decay to prevent unbounded growth
        grief->functional_impairment = exponential_decay(
            grief->functional_impairment, 0.2F, impairment_decay_rate, dt);  // Floor at 0.2
    } else if (grief->predominant_coping == COPING_AVOIDANT) {
        // Avoidant coping maintains elevated impairment (not dealing with grief)
        impairment_decay_rate *= 0.15F;  // Very slow recovery
        // Maintain a floor of impairment with avoidance
        float avoidance_impairment_floor = 0.55F * grief->avoidance_level;  // Higher floor
        grief->functional_impairment = exponential_decay(
            grief->functional_impairment, avoidance_impairment_floor, impairment_decay_rate, dt);
        // SAFETY: Cap at 0.85 even with high avoidance
        grief->functional_impairment = nimcp_clampf(grief->functional_impairment, 0.0F, 0.85F);
    } else {
        // Adaptive coping allows normal recovery
        grief->functional_impairment = exponential_decay(
            grief->functional_impairment, 0.0F, impairment_decay_rate, dt);
    }

    //=========================================================================
    // DUAL PROCESS OSCILLATION (Stroebe & Schut)
    //=========================================================================

    // WHAT: Alternate between loss-focused and restoration-focused coping
    // WHY:  Healthy grief involves both confronting loss AND rebuilding life
    // HOW:  Oscillate with decreasing loss-focus over time

    // Use prime number period to avoid sampling artifacts
    float oscillation_period = 3607.0F;  // ~1 hour (prime number to avoid alignment)
    float time_in_cycle = fmodf(time_since_loss, oscillation_period);
    float cycle_phase = time_in_cycle / oscillation_period;  // [0, 1]

    // Early grief: mostly loss-oriented
    // Later grief: more restoration-oriented
    float restoration_bias = time_since_loss / (86400.0F * 180.0F);  // Grows over 6 months
    restoration_bias = nimcp_clampf(restoration_bias, 0.0F, 0.8F);

    // Oscillate between loss and restoration with sinusoidal pattern
    // Range: [0.1, 0.9] for loss orientation (never fully 0 or 1)
    grief->loss_orientation = (1.0F - restoration_bias) * (0.5F + 0.4F * sinf(cycle_phase * 2.0F * M_PI));
    grief->restoration_orientation = 1.0F - grief->loss_orientation;

    //=========================================================================
    // NEUROBIOLOGICAL RECOVERY
    //=========================================================================

    // Serotonin recovers over months
    float serotonin_recovery_rate = 1.0F / (86400.0F * 90.0F);  // 3 months
    grief->serotonin_depletion = exponential_decay(
        grief->serotonin_depletion, 0.0F, serotonin_recovery_rate, dt);

    // Dopamine recovers as anhedonia lifts
    float dopamine_recovery_rate = 1.0F / (86400.0F * 60.0F);  // 2 months
    grief->dopamine_depletion = exponential_decay(
        grief->dopamine_depletion, 0.0F, dopamine_recovery_rate, dt);

    // Stress hormones normalize faster
    float stress_recovery_rate = 1.0F / (86400.0F * 30.0F);  // 1 month
    grief->norepinephrine_elevation = exponential_decay(
        grief->norepinephrine_elevation, 1.0F, stress_recovery_rate, dt);
    grief->cortisol_elevation = exponential_decay(
        grief->cortisol_elevation, 1.0F, stress_recovery_rate, dt);

    //=========================================================================
    // PROLONGED GRIEF CHECK
    //=========================================================================

    // Prolonged grief disorder: intense grief persisting > 6 months (death) or > 12 months (other)
    bool is_death = false;  // Need to check loss type
    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GRIEF_MAX_ATTACHMENTS > 256) {
            grief_and_loss_heartbeat("grief_and_lo_loop",
                             (float)(i + 1) / (float)GRIEF_MAX_ATTACHMENTS);
        }

        if (system->attachments[i].attachment_id == grief->lost_attachment_id) {
            is_death = (system->attachments[i].loss_type == LOSS_TYPE_DEATH);
            break;
        }
    }

    float prolonged_threshold = is_death ? (86400.0F * 180.0F) : (86400.0F * 365.0F);

    // Maladaptive/avoidant coping gradually increases prolonged grief severity
    if (grief->predominant_coping == COPING_MALADAPTIVE) {
        grief->prolonged_grief_severity += 0.15F * dt / (86400.0F * 30.0F);  // 0.15 per month (increased from 0.1)
        grief->prolonged_grief_severity = nimcp_clampf(grief->prolonged_grief_severity, 0.0F, 1.0F);
    } else if (grief->predominant_coping == COPING_AVOIDANT && time_since_loss > 86400.0F * 60.0F) {
        grief->prolonged_grief_severity += 0.08F * dt / (86400.0F * 30.0F);  // 0.08 per month after 2 months (increased from 0.05, earlier trigger)
        grief->prolonged_grief_severity = nimcp_clampf(grief->prolonged_grief_severity, 0.0F, 1.0F);
    } else if (grief->predominant_coping == COPING_ADAPTIVE) {
        // Adaptive coping reduces severity
        grief->prolonged_grief_severity *= 0.99F;  // Slow reduction
        if (grief->prolonged_grief_severity < 0.1F) {
            grief->prolonged_grief_risk = false;  // No longer at risk
        }
    }

    if (time_since_loss > prolonged_threshold) {
        // Check if symptoms persist at high intensity
        bool intense_pain = (grief->emotional_pain_intensity > 0.7F);
        bool high_impairment = (grief->functional_impairment > 0.6F);
        bool persistent_preoccupation = (grief->intrusive_thoughts_frequency > 0.7F);

        if (intense_pain && high_impairment && persistent_preoccupation) {
            grief->prolonged_grief_risk = true;
            grief->prolonged_grief_severity = fmaxf(grief->prolonged_grief_severity, 0.8F);
            system->complicated_grief_episodes++;
        }
    }

    //=========================================================================
    // SADNESS EMOTION UPDATE (Phase E1.1)
    //=========================================================================

    // WHAT: Update grief-induced sadness based on emotional pain and depression
    // WHY:  Sadness tracks with grief intensity
    // HOW:  Combine pain intensity and depression stage, update emotional tag

    // Grief-induced sadness = combination of pain and depression stage
    float depression_contribution = grief->stage_intensities[GRIEF_STAGE_DEPRESSION] * 0.7F;
    float acute_sadness = (grief->emotional_pain_intensity * 0.6F) + (depression_contribution * 0.4F);

    // SADNESS DECAY: Slowly decrease over time but never completely fade (like humans)
    // Calculate permanent sadness baseline from bond strength and loss type
    float permanent_sadness_baseline = 0.0F;
    for (int i = 0; i < GRIEF_MAX_ATTACHMENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GRIEF_MAX_ATTACHMENTS > 256) {
            grief_and_loss_heartbeat("grief_and_lo_loop",
                             (float)(i + 1) / (float)GRIEF_MAX_ATTACHMENTS);
        }

        if (system->attachments[i].attachment_id == grief->lost_attachment_id && system->attachments[i].severed) {
            // Permanent baseline proportional to bond strength and loss permanence
            float bond_factor = system->attachments[i].strength * 0.15F;  // Up to 15% baseline
            float loss_permanence = (system->attachments[i].loss_type == LOSS_TYPE_DEATH) ? 1.0F : 0.5F;
            permanent_sadness_baseline = bond_factor * loss_permanence;
            break;
        }
    }

    // Decay toward permanent baseline (never fully to zero)
    float sadness_decay_rate = 1.0F / (86400.0F * 365.0F);  // 1-year half-life
    grief->grief_induced_sadness = exponential_decay(
        grief->grief_induced_sadness,
        permanent_sadness_baseline,  // Target: permanent baseline, not zero
        sadness_decay_rate,
        dt
    );

    // Acute sadness can spike above baseline
    grief->grief_induced_sadness = fmaxf(grief->grief_induced_sadness, acute_sadness);
    grief->grief_induced_sadness = nimcp_clampf(grief->grief_induced_sadness, permanent_sadness_baseline, 1.0F);

    // Update sadness emotional tag (valence and arousal computed from current state)
    grief->sadness_emotion = grief_get_sadness_emotion(system);

    //=========================================================================
    // WISDOM ACCUMULATION (Incremental learning from grief)
    //=========================================================================

    // WHAT: Accumulate wisdom gradually through grief process
    // WHY:  Growth happens throughout grief, not just at "completion"
    // HOW:  Small increments based on meaning-making and acceptance

    if (grief->experiencing_grief) {
        // Wisdom grows with meaning-making and acceptance
        float wisdom_increment = grief->meaning_making_progress * grief->stage_intensities[GRIEF_STAGE_ACCEPTANCE] * dt / (86400.0F * 30.0F);
        system->accumulated_grief_wisdom += wisdom_increment;
        system->accumulated_grief_wisdom = nimcp_clampf(system->accumulated_grief_wisdom, 0.0F, 1.0F);
    }

    //=========================================================================
    // INTEGRATION AND COMPLETION
    //=========================================================================

    // Check if grief has integrated (acceptance + low pain + meaning-making)
    bool has_accepted = (grief->stage_intensities[GRIEF_STAGE_ACCEPTANCE] > 0.6F);
    bool pain_manageable = (grief->emotional_pain_intensity < 0.3F);
    bool found_meaning = (grief->meaning_making_progress > 0.5F);

    if (has_accepted && pain_manageable && found_meaning) {
        // Grief integrated - but never truly "over"
        grief->experiencing_grief = false;

        // Completion bonus wisdom
        system->accumulated_grief_wisdom += 0.05F;
        system->accumulated_grief_wisdom = nimcp_clampf(system->accumulated_grief_wisdom, 0.0F, 1.0F);

        // Update average duration
        float this_duration = time_since_loss;
        float total_grief_time = system->average_grief_duration * (float)(system->lifetime_losses - 1);
        system->average_grief_duration = (total_grief_time + this_duration) / (float)system->lifetime_losses;
    }

    system->total_update_calls++;
}

//=============================================================================
// EXISTENTIAL AWARENESS
//=============================================================================

void grief_contemplate_mortality(grief_system_t* system,
                                float mortality_reminder_intensity,
                                uint64_t current_time_us) {
    // WHAT: Process awareness of death and finitude
    // WHY:  Death awareness shapes behavior, values, meaning
    // HOW:  Update mortality salience, death anxiety, existential concerns

    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_grief_contemplate_mo", 0.0f);


    existential_state_t* ex = &system->existential;

    // Awakens mortality awareness
    if (!ex->aware_of_mortality && mortality_reminder_intensity > 0.3F) {
        ex->aware_of_mortality = true;
    }

    // Mortality salience increases with reminders
    ex->mortality_salience += mortality_reminder_intensity * 0.3F;
    ex->mortality_salience = nimcp_clampf(ex->mortality_salience, 0.0F, 1.0F);
    ex->last_mortality_reminder = current_time_us;

    // Death anxiety may increase or decrease depending on acceptance
    if (ex->acceptance_of_finitude < 0.5F) {
        // Low acceptance → more anxiety (increased threshold to 0.5)
        ex->death_anxiety += mortality_reminder_intensity * 0.2F;
    } else {
        // High acceptance → less anxiety
        ex->death_anxiety -= mortality_reminder_intensity * 0.1F;
    }
    ex->death_anxiety = nimcp_clampf(ex->death_anxiety, 0.0F, 1.0F);

    // Existential anxiety (meaninglessness) increases with mortality salience
    // and low-to-moderate sense of purpose (confronting death raises existential questions)
    if (ex->sense_of_purpose < 0.7F) {
        ex->existential_anxiety += mortality_reminder_intensity * 0.15F;
    }
    ex->existential_anxiety = nimcp_clampf(ex->existential_anxiety, 0.0F, 1.0F);

    // Legacy concerns increase with mortality awareness
    ex->legacy_motivation = ex->mortality_salience * 0.7F;
}

void grief_find_meaning(grief_system_t* system, float meaning_making_effort) {
    // WHAT: Engage in meaning-making after loss
    // WHY:  Meaning transforms suffering into growth
    // HOW:  Increase purpose, acceptance, reduce anxiety

    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_grief_find_meaning", 0.0f);


    grief_state_t* grief = &system->current_grief;
    existential_state_t* ex = &system->existential;

    // Meaning-making progress
    grief->meaning_making_progress += meaning_making_effort * 0.1F;
    grief->meaning_making_progress = nimcp_clampf(grief->meaning_making_progress, 0.0F, 1.0F);

    // Increases sense of purpose
    ex->sense_of_purpose += meaning_making_effort * 0.05F;
    ex->sense_of_purpose = nimcp_clampf(ex->sense_of_purpose, 0.0F, 1.0F);

    // Reduces existential anxiety
    ex->existential_anxiety -= meaning_making_effort * 0.05F;
    ex->existential_anxiety = nimcp_clampf(ex->existential_anxiety, 0.0F, 1.0F);

    // Increases acceptance of finitude
    ex->acceptance_of_finitude += meaning_making_effort * 0.03F;
    ex->acceptance_of_finitude = nimcp_clampf(ex->acceptance_of_finitude, 0.0F, 1.0F);

    // Increases generativity (creating for future generations)
    ex->generativity += meaning_making_effort * 0.08F;
    ex->generativity = nimcp_clampf(ex->generativity, 0.0F, 1.0F);

    // Legacy motivation (desire to leave impact)
    ex->legacy_motivation += meaning_making_effort * 0.06F;
    ex->legacy_motivation = nimcp_clampf(ex->legacy_motivation, 0.0F, 1.0F);

    // Facilitates movement toward acceptance stage
    grief->stage_intensities[GRIEF_STAGE_ACCEPTANCE] += meaning_making_effort * 0.05F;
    grief->stage_intensities[GRIEF_STAGE_MEANING_MAKING] += meaning_making_effort * 0.1F;
}

//=============================================================================
// COPING MECHANISMS
//=============================================================================

void grief_seek_support(grief_system_t* system, float support_quality) {
    // WHAT: Reach out for social support
    // WHY:  Social connection aids grief recovery
    // HOW:  Reduces pain, increases restoration orientation

    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_grief_seek_support", 0.0f);


    grief_state_t* grief = &system->current_grief;

    grief->social_support_seeking = support_quality;
    grief->predominant_coping = COPING_ADAPTIVE;

    // Support reduces pain and impairment
    grief->emotional_pain_intensity *= (1.0F - support_quality * 0.1F);
    grief->functional_impairment *= (1.0F - support_quality * 0.15F);

    // Increases restoration orientation
    grief->restoration_orientation += support_quality * 0.1F;
    grief->restoration_orientation = nimcp_clampf(grief->restoration_orientation, 0.0F, 1.0F);
}

void grief_avoid_reminders(grief_system_t* system, float avoidance_intensity) {
    // WHAT: Engage in avoidant coping
    // WHY:  Sometimes protective, but can become maladaptive
    // HOW:  Reduces intrusive thoughts short-term, but prolongs grief

    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_grief_avoid_reminder", 0.0f);


    grief_state_t* grief = &system->current_grief;

    grief->avoidance_level = avoidance_intensity;

    // Short-term relief
    grief->intrusive_thoughts_frequency *= (1.0F - avoidance_intensity * 0.2F);

    // But if sustained, becomes maladaptive
    if (avoidance_intensity > 0.7F) {
        grief->predominant_coping = COPING_AVOIDANT;
        // Prolongs grief
        grief->prolonged_grief_risk = true;
    }
}

void grief_express_emotions(grief_system_t* system, float expression_intensity) {
    // WHAT: Process emotions expressively (crying, talking, writing)
    // WHY:  Emotional expression aids integration
    // HOW:  Reduces pain, facilitates stage progression

    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_grief_express_emotio", 0.0f);


    grief_state_t* grief = &system->current_grief;

    grief->predominant_coping = COPING_ADAPTIVE;

    // Reduces pain through catharsis
    grief->emotional_pain_intensity *= (1.0F - expression_intensity * 0.05F);

    // Facilitates acceptance
    grief->stage_intensities[GRIEF_STAGE_ACCEPTANCE] += expression_intensity * 0.02F;

    // Reduces prolonged grief risk
    if (expression_intensity > 0.5F) {
        grief->prolonged_grief_severity *= 0.95F;
    }
}

//=============================================================================
// QUERY FUNCTIONS
//=============================================================================

bool grief_is_grieving(const grief_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_grief_is_grieving", 0.0f);


    return system ? system->current_grief.experiencing_grief : false;
}

float grief_get_pain_intensity(const grief_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_grief_get_pain_inten", 0.0f);


    return system ? system->current_grief.emotional_pain_intensity : 0.0F;
}

grief_stage_t grief_get_current_stage(const grief_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_grief_get_current_st", 0.0f);


    return system ? system->current_grief.current_stage : GRIEF_STAGE_SHOCK;
}

bool grief_has_prolonged_grief_risk(const grief_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_grief_has_prolonged_", 0.0f);


    return system ? system->current_grief.prolonged_grief_risk : false;
}

void grief_get_neuromodulator_effects(const grief_system_t* system,
                                     float* serotonin_factor,
                                     float* dopamine_factor,
                                     float* norepinephrine_factor) {
    // WHAT: Get multipliers for neuromodulator integration
    // WHY:  Grief affects brain chemistry
    // HOW:  Return depletion/elevation factors

    if (!system || !system->current_grief.experiencing_grief) {
        if (serotonin_factor) *serotonin_factor = 1.0F;
        if (dopamine_factor) *dopamine_factor = 1.0F;
        if (norepinephrine_factor) *norepinephrine_factor = 1.0F;
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_grief_get_neuromodul", 0.0f);


    const grief_state_t* grief = &system->current_grief;

    // Serotonin depleted → multiply by (1 - depletion)
    if (serotonin_factor) {
        *serotonin_factor = 1.0F - grief->serotonin_depletion;
    }

    // Dopamine depleted
    if (dopamine_factor) {
        *dopamine_factor = 1.0F - grief->dopamine_depletion;
    }

    // Norepinephrine elevated → multiply by elevation factor
    if (norepinephrine_factor) {
        *norepinephrine_factor = grief->norepinephrine_elevation;
    }
}

//=============================================================================
// EMOTION INTEGRATION (Phase E1.1: Sadness)
//=============================================================================

emotional_tag_t grief_get_sadness_emotion(const grief_system_t* system) {
    /* WHAT: Get sadness emotional state from grief
     * WHY:  Integration with emotional tagging system
     * HOW:  Compute valence and arousal from grief intensity
     *
     * SADNESS MAPPING (Russell's Circumplex Model):
     * - Valence: Negative, proportional to grief intensity [-0.3 to -0.9]
     * - Arousal: Low during sadness (vs high during anxiety/anger) [0.1 to 0.4]
     * - Category: EMOTION_SADNESS (valence < -0.3, arousal < 0.4)
     *
     * BIOLOGICAL: Sadness has low arousal (decreased energy, psychomotor retardation)
     * REFERENCE: Russell (1980) - Circumplex model of affect
     */

    if (!system || !system->current_grief.experiencing_grief) {
        // Not grieving: return neutral emotion
        return emotional_tag_neutral();
    }

    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_grief_get_sadness_em", 0.0f);


    const grief_state_t* grief = &system->current_grief;

    // Valence: More negative with higher grief intensity
    // Emotional pain [0, 1] → valence [-0.3, -0.9]
    float valence = -0.3F - (grief->emotional_pain_intensity * 0.6F);
    valence = nimcp_clampf(valence, -1.0F, -0.3F);

    // Arousal: Low during sadness (psychomotor retardation)
    // Depression and sadness are characterized by low energy
    // Arousal decreases with depression, increases slightly with anxiety
    float baseline_arousal = 0.2F;  // Low baseline for sadness
    float anxiety_boost = grief->stage_intensities[GRIEF_STAGE_ANGER] * 0.2F;  // Anger can increase arousal
    float arousal = baseline_arousal + anxiety_boost;
    arousal = nimcp_clampf(arousal, 0.0F, 0.4F);  // Sadness has low arousal

    // Get current time (use 0 if not available - will be updated by caller)
    uint64_t timestamp_ms = 0;

    // Create emotional tag (will auto-classify as EMOTION_SADNESS)
    return emotional_tag_create(valence, arousal, timestamp_ms);
}

float grief_get_total_sadness(const grief_system_t* system) {
    /* WHAT: Get combined sadness (baseline + grief-induced)
     * WHY:  Track total sadness burden
     * HOW:  Sum baseline and grief-induced, clamp to [0, 1]
     */

    if (!system) return 0.0F;

    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_grief_get_total_sadn", 0.0f);


    const grief_state_t* grief = &system->current_grief;

    // Total sadness = pre-existing + grief-induced
    float total = grief->baseline_sadness + grief->grief_induced_sadness;

    return nimcp_clampf(total, 0.0F, 1.0F);
}

void grief_set_baseline_sadness(grief_system_t* system, float sadness_level) {
    /* WHAT: Set pre-grief sadness level
     * WHY:  Grief compounds with existing depression/sadness
     * HOW:  Update baseline, clamp to valid range
     */

    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_grief_set_baseline_s", 0.0f);


    system->current_grief.baseline_sadness = nimcp_clampf(sadness_level, 0.0F, 1.0F);
}

/* ============================================================================
 * W10 (2026-04-24): KG runtime emit for grief stage transitions.
 *
 * Callers that hold a brain_t + grief_system_t invoke this to snapshot the
 * current stage+pain intensity into brain->internal_kg and query the pain
 * bias (read-path for downstream modulation).
 * ============================================================================ */
float grief_and_loss_wave10_kg_sync(struct brain_struct* brain,
                                    const grief_system_t* system,
                                    int from_stage)
{
    if (!brain || !system) return 0.5f;
    int to_stage = (int)grief_get_current_stage(system);
    float pain   = grief_get_pain_intensity(system);
    wave10_grief_emit_stage_transition(brain, from_stage, to_stage, pain);
    return wave10_grief_query_pain_bias(brain);
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int grief_and_loss_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    grief_and_loss_heartbeat("grief_and_lo_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Grief_And_Loss");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                grief_and_loss_heartbeat("grief_and_lo_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Grief_And_Loss");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Grief_And_Loss");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Instance-Level Health Agent (Phase 8 Utility Integration)
 * ============================================================================ */


void grief_and_loss_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_grief_and_loss_instance_health_agent = agent;
}

/* ============================================================================
 * Training Stubs (Phase 8 Utility Integration)
 *
 * Stub: training integration planned — these are intentional no-ops that
 * provide heartbeat signaling only. Full training hooks will wire into the
 * training-immune bridge when per-module gradient propagation is implemented.
 * ============================================================================ */

int grief_and_loss_training_begin(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "grief_and_loss_training_begin: ctx is NULL");
        return -1;
    }
    grief_and_loss_heartbeat_instance(g_grief_and_loss_instance_health_agent, "grief_and_loss_training_begin", 0.0f);
    return 0;
}

int grief_and_loss_training_end(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "grief_and_loss_training_end: ctx is NULL");
        return -1;
    }
    grief_and_loss_heartbeat_instance(g_grief_and_loss_instance_health_agent, "grief_and_loss_training_end", 1.0f);
    return 0;
}

int grief_and_loss_training_step(void* ctx, float progress) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "grief_and_loss_training_step: ctx is NULL");
        return -1;
    }
    grief_and_loss_heartbeat_instance(g_grief_and_loss_instance_health_agent, "grief_and_loss_training_step", progress);
    return 0;
}
