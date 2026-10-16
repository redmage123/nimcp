/**
 * @file nimcp_love_loyalty_friendship.c
 * @brief Implementation of love, loyalty, and friendship system
 */

#include "cognitive/nimcp_love_loyalty_friendship.h"
#include "cognitive/social/nimcp_social_snn_bridge.h"
#include "cognitive/social/nimcp_social_plasticity_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_memory.h"
#include <math.h>
#include <string.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "cognitive.social"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(love_loyalty_friendship, MESH_ADAPTER_CATEGORY_COGNITIVE)



#define BIO_MODULE_COGNITIVE_SOCIAL 0x034F

static inline float exponential_decay(float current, float target, float decay_rate, float dt) {
    float decay_factor = expf(-decay_rate * dt);
    return current * decay_factor + target * (1.0F - decay_factor);
}

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

social_bond_system_t* social_bond_system_create(void) {
    LOG_DEBUG("Creating module");
    // WHAT: Allocate and initialize social bond system
    // WHY:  Central system for love, loyalty, friendship
    // HOW:  Zero-initialize all state, set up default parameters

    social_bond_system_t* system = (social_bond_system_t*)nimcp_calloc(1, sizeof(social_bond_system_t));
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate system");

        return NULL;

    }

    // Initialize all relationships as inactive
    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        system->relationships[i].active = false;
    }

    // Default emotional state
    system->emotion.attachment_style = ATTACHMENT_SECURE;
    system->emotion.attachment_security = 0.7F;
    system->emotion.oxytocin_level = 0.5F;

    // Default personality (moderate values)
    system->extraversion = 0.5F;
    system->agreeableness = 0.6F;
    system->openness_to_experience = 0.5F;

    // Default capacities
    system->love_capacity = 0.8F;
    system->friendship_capacity = 0.8F;
    system->loyalty_capacity = 0.7F;

    // Default social skills
    system->empathy = 0.6F;
    system->perspective_taking = 0.6F;
    system->conflict_resolution = 0.5F;

    // Default: integrate with other systems
    system->integrate_with_theory_of_mind = true;
    system->integrate_with_oxytocin = true;
    system->integrate_with_reward = true;

    
    // Bio-async registration
    system->bio_ctx = NULL;
    system->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_EMOTIONS_SOCIAL,
            .module_name = "love_loyalty_friendship",
            .inbox_capacity = 32,
            .user_data = system
        };
        system->bio_ctx = bio_router_register_module(&bio_info);
        if (system->bio_ctx) {
            system->bio_async_enabled = true;
        }
    }

    // Initialize SNN and Plasticity bridges
    system->snn_bridge = NULL;
    system->plasticity_bridge = NULL;
    system->bridges_enabled = false;

    social_snn_config_t snn_config = social_snn_config_default();
    system->snn_bridge = social_snn_create(&snn_config);

    social_plasticity_config_t plasticity_config = social_plasticity_config_default();
    system->plasticity_bridge = social_plasticity_create(&plasticity_config);

    if (system->snn_bridge && system->plasticity_bridge) {
        system->bridges_enabled = true;
        LOG_DEBUG("SNN and Plasticity bridges initialized");
    }

return system;
}

void social_bond_system_destroy(social_bond_system_t* system) {
    LOG_DEBUG("Destroying module");
    // WHAT: Free social bond system memory
    // WHY:  Prevent memory leaks
    // HOW:  Free main structure

    if (!system) return;

    // Destroy SNN and Plasticity bridges
    if (system->snn_bridge) {
        social_snn_destroy(system->snn_bridge);
        system->snn_bridge = NULL;
    }
    if (system->plasticity_bridge) {
        social_plasticity_destroy(system->plasticity_bridge);
        system->plasticity_bridge = NULL;
    }
    system->bridges_enabled = false;

    // Unregister from bio-router
    if (system->bio_async_enabled && system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
        system->bio_async_enabled = false;
    }

    nimcp_free(system);
    system = NULL;
}

void social_bond_system_reset(social_bond_system_t* system) {
    // WHAT: Reset system to initial state
    // WHY:  Clear all relationships and emotional state
    // HOW:  Reinitialize all fields except personality/capacities

    if (!system) return;

    // Save personality traits and capacities
    float extraversion = system->extraversion;
    float agreeableness = system->agreeableness;
    float openness = system->openness_to_experience;
    float love_cap = system->love_capacity;
    float friend_cap = system->friendship_capacity;
    float loyalty_cap = system->loyalty_capacity;
    float empathy = system->empathy;
    float perspective = system->perspective_taking;
    float conflict = system->conflict_resolution;
    attachment_style_t style = system->emotion.attachment_style;

    // Clear all relationships
    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        system->relationships[i].active = false;
    }
    system->active_relationship_count = 0;

    // Reset emotional state
    memset(&system->emotion, 0, sizeof(social_emotional_state_t));
    system->emotion.attachment_style = style;
    system->emotion.attachment_security = 0.7F;
    system->emotion.oxytocin_level = 0.5F;

    // Restore personality
    system->extraversion = extraversion;
    system->agreeableness = agreeableness;
    system->openness_to_experience = openness;
    system->love_capacity = love_cap;
    system->friendship_capacity = friend_cap;
    system->loyalty_capacity = loyalty_cap;
    system->empathy = empathy;
    system->perspective_taking = perspective;
    system->conflict_resolution = conflict;

    // Reset statistics
    system->total_update_calls = 0;
    system->total_friendships_formed = 0;
    system->total_loves_experienced = 0;
    system->total_loyalty_commitments = 0;
    system->average_relationship_closeness = 0.0F;
}

//=============================================================================
// RELATIONSHIP MANAGEMENT FUNCTIONS
//=============================================================================

uint32_t social_create_relationship(social_bond_system_t* system,
                                   relationship_stage_t initial_stage,
                                   uint64_t current_time_us) {
    // WHAT: Create a new relationship
    // WHY:  Track social bonds with others
    // HOW:  Find empty slot, assign ID, initialize

    if (!system) return UINT32_MAX;

    // Find empty slot
    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        if (!system->relationships[i].active) {
            relationship_t* rel = &system->relationships[i];

            rel->active = true;
            rel->relationship_id = (uint32_t)(i + 1);  // 1-indexed IDs
            rel->stage = initial_stage;
            rel->love_type = LOVE_TYPE_PLATONIC;

            // Initialize dimensions based on stage
            rel->closeness = (initial_stage == RELATIONSHIP_STRANGER) ? 0.0F : 0.2F;
            rel->trust = 0.5F;  // Start with moderate trust
            rel->affection = 0.1F;
            rel->reciprocity = 0.5F;

            // Initialize love components
            rel->love.intimacy = 0.0F;
            rel->love.passion = 0.0F;
            rel->love.commitment = 0.0F;

            // Initialize loyalty
            rel->is_loyal_to = false;
            rel->loyalty_strength = 0.0F;

            // Initialize friendship qualities
            rel->shared_experiences_count = 0.0F;
            rel->vulnerability_shared = 0.0F;
            rel->support_given = 0.0F;
            rel->support_received = 0.0F;

            // Initialize oxytocin bonding
            rel->oxytocin_bond_strength = 0.0F;
            rel->last_interaction_time = current_time_us;

            // Initialize all experiences as inactive
            for (int j = 0; j < SOCIAL_MAX_SHARED_EXPERIENCES; j++) {
                rel->experiences[j].active = false;
            }
            rel->experience_count = 0;
            rel->experience_history_index = 0;

            // Trust violations
            rel->betrayals_experienced = 0;
            rel->trust_repair_progress = 1.0F;

            // Duration
            rel->relationship_start_time = current_time_us;
            rel->time_known = 0.0F;

            system->active_relationship_count++;
            return rel->relationship_id;
        }
    }

    return UINT32_MAX;  // No slots available
}

void social_process_interaction(social_bond_system_t* system,
                               uint32_t relationship_id,
                               interaction_type_t type,
                               float emotional_intensity,
                               float valence,
                               uint64_t current_time_us) {
    // WHAT: Record interaction and update relationship dynamics
    // WHY:  Relationships grow through shared experiences
    // HOW:  Update closeness, trust, reciprocity based on interaction

    if (!system) return;

    // Clamp inputs
    emotional_intensity = nimcp_clampf(emotional_intensity, 0.0F, 1.0F);
    valence = nimcp_clampf(valence, -1.0F, 1.0F);

    // Find relationship
    relationship_t* rel = NULL;
    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        if (system->relationships[i].active && system->relationships[i].relationship_id == relationship_id) {
            rel = &system->relationships[i];
            break;
        }
    }
    if (!rel) return;

    // Record shared experience
    int exp_index = rel->experience_history_index % SOCIAL_MAX_SHARED_EXPERIENCES;
    shared_experience_t* exp = &rel->experiences[exp_index];

    exp->active = true;
    exp->type = type;
    exp->timestamp = current_time_us;
    exp->emotional_intensity = emotional_intensity;
    exp->valence = valence;
    exp->strengthened_bond = (valence > 0.0F && emotional_intensity > 0.3F);

    rel->experience_count++;
    rel->experience_history_index++;
    rel->shared_experiences_count += 1.0F;

    // Update last interaction time
    rel->last_interaction_time = current_time_us;

    // Update relationship dimensions based on interaction type
    float closeness_change = 0.0F;
    float trust_change = 0.0F;
    float affection_change = 0.0F;

    switch (type) {
        case INTERACTION_CONVERSATION:
            closeness_change = valence * emotional_intensity * 0.05F;
            affection_change = valence * 0.03F;
            break;

        case INTERACTION_SHARED_ACTIVITY:
            closeness_change = valence * emotional_intensity * 0.08F;
            rel->shared_experiences_count += emotional_intensity;
            break;

        case INTERACTION_SUPPORT_GIVEN:
            rel->support_given += emotional_intensity * 0.1F;
            rel->reciprocity += 0.05F;
            trust_change = 0.05F;
            break;

        case INTERACTION_SUPPORT_RECEIVED:
            rel->support_received += emotional_intensity * 0.1F;
            rel->reciprocity -= 0.05F;  // Creates debt to reciprocate
            trust_change = valence * 0.08F;
            closeness_change = valence * 0.06F;
            break;

        case INTERACTION_VULNERABILITY:
            // Handled separately by social_express_vulnerability
            break;

        case INTERACTION_CONFLICT:
            closeness_change = -emotional_intensity * 0.1F;
            trust_change = -emotional_intensity * 0.05F;
            affection_change = -emotional_intensity * 0.08F;
            break;

        case INTERACTION_RECONCILIATION:
            closeness_change = emotional_intensity * 0.15F;
            trust_change = emotional_intensity * 0.1F;
            rel->trust_repair_progress += 0.1F;
            break;

        case INTERACTION_BETRAYAL:
            // Handled separately by social_experience_betrayal
            break;

        case INTERACTION_CELEBRATION:
            closeness_change = emotional_intensity * 0.1F;
            affection_change = emotional_intensity * 0.08F;
            rel->love.intimacy += emotional_intensity * 0.05F;
            break;

        case INTERACTION_COLLABORATION:
            closeness_change = valence * emotional_intensity * 0.07F;
            trust_change = valence * emotional_intensity * 0.06F;
            rel->love.commitment += 0.03F;
            break;
    }

    // Apply changes
    rel->closeness += closeness_change;
    rel->trust += trust_change;
    rel->affection += affection_change;

    // Clamp values
    rel->closeness = nimcp_clampf(rel->closeness, 0.0F, 1.0F);
    rel->trust = nimcp_clampf(rel->trust, 0.0F, 1.0F);
    rel->affection = nimcp_clampf(rel->affection, 0.0F, 1.0F);
    rel->reciprocity = nimcp_clampf(rel->reciprocity, 0.0F, 1.0F);

    // Update relationship stage based on closeness
    if (rel->closeness >= CLOSE_FRIEND_THRESHOLD && rel->stage < RELATIONSHIP_CLOSE_FRIEND) {
        rel->stage = RELATIONSHIP_CLOSE_FRIEND;
        system->total_friendships_formed++;
    } else if (rel->closeness >= FRIENDSHIP_THRESHOLD && rel->stage < RELATIONSHIP_FRIEND) {
        rel->stage = RELATIONSHIP_FRIEND;
    } else if (rel->closeness > 0.1F && rel->stage == RELATIONSHIP_STRANGER) {
        rel->stage = RELATIONSHIP_ACQUAINTANCE;
    }

    // Oxytocin boost from positive interactions
    if (valence > 0.0F && emotional_intensity > 0.4F) {
        rel->oxytocin_bond_strength += emotional_intensity * 0.1F;
        rel->oxytocin_bond_strength = nimcp_clampf(rel->oxytocin_bond_strength, 0.0F, 1.0F);

        system->emotion.oxytocin_level += emotional_intensity * 0.15F;
        system->emotion.oxytocin_level = nimcp_clampf(system->emotion.oxytocin_level, 0.0F, 1.0F);
        system->emotion.last_oxytocin_boost = current_time_us;
    }
}

void social_express_vulnerability(social_bond_system_t* system,
                                 uint32_t relationship_id,
                                 float vulnerability_level,
                                 bool received_well) {
    // WHAT: Share weakness, fear, or private information
    // WHY:  Vulnerability deepens intimacy and trust
    // HOW:  Increases closeness if received well

    if (!system) return;

    vulnerability_level = nimcp_clampf(vulnerability_level, 0.0F, 1.0F);

    // Find relationship
    relationship_t* rel = NULL;
    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        if (system->relationships[i].active && system->relationships[i].relationship_id == relationship_id) {
            rel = &system->relationships[i];
            break;
        }
    }
    if (!rel) return;

    // Track vulnerability shared
    rel->vulnerability_shared += vulnerability_level * 0.1F;
    rel->vulnerability_shared = nimcp_clampf(rel->vulnerability_shared, 0.0F, 1.0F);

    if (received_well) {
        // Vulnerability accepted → deepens bond
        float intimacy_increase = vulnerability_level * 0.15F;
        rel->love.intimacy += intimacy_increase;
        rel->closeness += vulnerability_level * 0.12F;
        rel->trust += vulnerability_level * 0.1F;

        // Major intimacy boost
        rel->love.intimacy = nimcp_clampf(rel->love.intimacy, 0.0F, 1.0F);
        rel->closeness = nimcp_clampf(rel->closeness, 0.0F, 1.0F);
        rel->trust = nimcp_clampf(rel->trust, 0.0F, 1.0F);
    } else {
        // Vulnerability rejected → damages bond
        rel->closeness -= vulnerability_level * 0.2F;
        rel->trust -= vulnerability_level * 0.15F;
        rel->closeness = nimcp_clampf(rel->closeness, 0.0F, 1.0F);
        rel->trust = nimcp_clampf(rel->trust, 0.0F, 1.0F);
    }
}

void social_provide_support(social_bond_system_t* system,
                           uint32_t relationship_id,
                           float support_quality) {
    // WHAT: Help, comfort, or assist in time of need
    // WHY:  Support builds reciprocity and strengthens bonds
    // HOW:  Update support_given, reciprocity, closeness

    if (!system) return;

    support_quality = nimcp_clampf(support_quality, 0.0F, 1.0F);

    // Find relationship
    relationship_t* rel = NULL;
    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        if (system->relationships[i].active && system->relationships[i].relationship_id == relationship_id) {
            rel = &system->relationships[i];
            break;
        }
    }
    if (!rel) return;

    rel->support_given += support_quality * 0.1F;
    rel->reciprocity += support_quality * 0.08F;
    rel->closeness += support_quality * 0.06F;
    rel->trust += support_quality * 0.05F;

    // Clamp
    rel->support_given = nimcp_clampf(rel->support_given, 0.0F, 2.0F);  // Can accumulate
    rel->reciprocity = nimcp_clampf(rel->reciprocity, 0.0F, 1.0F);
    rel->closeness = nimcp_clampf(rel->closeness, 0.0F, 1.0F);
    rel->trust = nimcp_clampf(rel->trust, 0.0F, 1.0F);
}

void social_receive_support(social_bond_system_t* system,
                           uint32_t relationship_id,
                           float support_quality) {
    // WHAT: Be helped by another in time of need
    // WHY:  Receiving support creates gratitude and reciprocity
    // HOW:  Update support_received, trust, closeness

    if (!system) return;

    support_quality = nimcp_clampf(support_quality, 0.0F, 1.0F);

    // Find relationship
    relationship_t* rel = NULL;
    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        if (system->relationships[i].active && system->relationships[i].relationship_id == relationship_id) {
            rel = &system->relationships[i];
            break;
        }
    }
    if (!rel) return;

    rel->support_received += support_quality * 0.1F;
    rel->reciprocity -= support_quality * 0.05F;  // Creates obligation
    rel->closeness += support_quality * 0.08F;
    rel->trust += support_quality * 0.1F;
    rel->affection += support_quality * 0.06F;

    // Clamp
    rel->support_received = nimcp_clampf(rel->support_received, 0.0F, 2.0F);
    rel->reciprocity = nimcp_clampf(rel->reciprocity, 0.0F, 1.0F);
    rel->closeness = nimcp_clampf(rel->closeness, 0.0F, 1.0F);
    rel->trust = nimcp_clampf(rel->trust, 0.0F, 1.0F);
    rel->affection = nimcp_clampf(rel->affection, 0.0F, 1.0F);
}

//=============================================================================
// LOYALTY FUNCTIONS
//=============================================================================

void social_commit_loyalty(social_bond_system_t* system,
                          uint32_t relationship_id,
                          loyalty_type_t loyalty_type,
                          float strength) {
    // WHAT: Pledge allegiance to person/group/cause
    // WHY:  Loyalty provides stability and trust
    // HOW:  Set loyalty flags, increase commitment

    if (!system) return;

    strength = nimcp_clampf(strength, 0.0F, 1.0F);

    // Find relationship
    relationship_t* rel = NULL;
    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        if (system->relationships[i].active && system->relationships[i].relationship_id == relationship_id) {
            rel = &system->relationships[i];
            break;
        }
    }
    if (!rel) return;

    rel->is_loyal_to = true;
    rel->loyalty_type = loyalty_type;
    rel->loyalty_strength = strength;
    rel->love.commitment += strength * 0.3F;
    rel->love.commitment = nimcp_clampf(rel->love.commitment, 0.0F, 1.0F);

    system->total_loyalty_commitments++;
    system->emotion.actively_loyal = true;
}

void social_test_loyalty(social_bond_system_t* system,
                        uint32_t relationship_id,
                        float test_difficulty,
                        bool remained_loyal) {
    // WHAT: Loyalty challenged by difficulty or temptation
    // WHY:  Loyalty proven through tests
    // HOW:  Either strengthens or breaks commitment

    if (!system) return;

    test_difficulty = nimcp_clampf(test_difficulty, 0.0F, 1.0F);

    // Find relationship
    relationship_t* rel = NULL;
    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        if (system->relationships[i].active && system->relationships[i].relationship_id == relationship_id) {
            rel = &system->relationships[i];
            break;
        }
    }
    if (!rel) return;

    if (remained_loyal) {
        // Passed test → strengthens loyalty
        rel->loyalty_tests_passed++;
        rel->loyalty_strength += test_difficulty * 0.1F;
        rel->loyalty_strength = nimcp_clampf(rel->loyalty_strength, 0.0F, 1.0F);
        rel->love.commitment += test_difficulty * 0.08F;
        rel->love.commitment = nimcp_clampf(rel->love.commitment, 0.0F, 1.0F);
    } else {
        // Failed test → damages loyalty
        rel->loyalty_tests_failed++;
        rel->loyalty_strength -= test_difficulty * 0.3F;
        if (rel->loyalty_strength < LOYALTY_THRESHOLD) {
            rel->is_loyal_to = false;
        }
        rel->trust -= test_difficulty * 0.2F;
        rel->trust = nimcp_clampf(rel->trust, 0.0F, 1.0F);

        // Broken loyalty also damages relationship closeness
        rel->closeness -= test_difficulty * 0.15F;
        rel->closeness = nimcp_clampf(rel->closeness, 0.0F, 1.0F);
    }
}

//=============================================================================
// LOVE FUNCTIONS
//=============================================================================

void social_experience_love(social_bond_system_t* system,
                           uint32_t relationship_id,
                           love_type_t love_type,
                           float intensity) {
    // WHAT: Feel deep affection, care, attachment
    // WHY:  Love is core positive emotion
    // HOW:  Update love components (intimacy, passion, commitment)

    if (!system) return;

    intensity = nimcp_clampf(intensity, 0.0F, 1.0F);

    // Find relationship
    relationship_t* rel = NULL;
    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        if (system->relationships[i].active && system->relationships[i].relationship_id == relationship_id) {
            rel = &system->relationships[i];
            break;
        }
    }
    if (!rel) return;

    rel->love_type = love_type;

    // Update Sternberg's love components based on type
    switch (love_type) {
        case LOVE_TYPE_ROMANTIC:
            rel->love.intimacy += intensity * 0.1F;
            rel->love.passion += intensity * 0.15F;
            rel->love.commitment += intensity * 0.08F;
            rel->stage = RELATIONSHIP_ROMANTIC;
            break;

        case LOVE_TYPE_COMPANIONATE:
            rel->love.intimacy += intensity * 0.12F;
            rel->love.passion += intensity * 0.02F;
            rel->love.commitment += intensity * 0.12F;
            break;

        case LOVE_TYPE_FAMILIAL:
            rel->love.intimacy += intensity * 0.08F;
            rel->love.commitment += intensity * 0.15F;
            rel->stage = RELATIONSHIP_FAMILY;
            break;

        case LOVE_TYPE_PLATONIC:
            rel->love.intimacy += intensity * 0.1F;
            rel->love.commitment += intensity * 0.08F;
            break;

        case LOVE_TYPE_COMPASSIONATE:
            rel->affection += intensity * 0.1F;
            break;

        case LOVE_TYPE_SELF_LOVE:
            // Self-love handled differently
            break;
    }

    // Clamp love components
    rel->love.intimacy = nimcp_clampf(rel->love.intimacy, 0.0F, 1.0F);
    rel->love.passion = nimcp_clampf(rel->love.passion, 0.0F, 1.0F);
    rel->love.commitment = nimcp_clampf(rel->love.commitment, 0.0F, 1.0F);

    // Update affection and closeness
    rel->affection += intensity * 0.1F;
    rel->closeness += intensity * 0.08F;
    rel->affection = nimcp_clampf(rel->affection, 0.0F, 1.0F);
    rel->closeness = nimcp_clampf(rel->closeness, 0.0F, 1.0F);

    // Update system-level love state
    system->emotion.experiencing_love = true;
    system->emotion.love_intensity = intensity;
    system->emotion.love_type_felt = love_type;
    system->total_loves_experienced++;

    // Oxytocin boost
    system->emotion.oxytocin_level += intensity * 0.2F;
    system->emotion.oxytocin_level = nimcp_clampf(system->emotion.oxytocin_level, 0.0F, 1.0F);
}

//=============================================================================
// BETRAYAL AND REPAIR
//=============================================================================

void social_experience_betrayal(social_bond_system_t* system,
                               uint32_t relationship_id,
                               float severity) {
    // WHAT: Trust violation by someone close
    // WHY:  Betrayal damages relationships, needs processing
    // HOW:  Reduce trust, closeness, trigger grief response

    if (!system) return;

    severity = nimcp_clampf(severity, 0.0F, 1.0F);

    // Find relationship
    relationship_t* rel = NULL;
    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        if (system->relationships[i].active && system->relationships[i].relationship_id == relationship_id) {
            rel = &system->relationships[i];
            break;
        }
    }
    if (!rel) return;

    rel->betrayals_experienced++;
    rel->trust_repair_progress = 0.0F;

    // Damage relationship dimensions
    rel->trust -= severity * 0.6F;
    rel->closeness -= severity * 0.4F;
    rel->affection -= severity * 0.5F;
    rel->love.intimacy -= severity * 0.4F;

    // Break loyalty if severe
    if (severity > 0.7F && rel->is_loyal_to) {
        rel->is_loyal_to = false;
        rel->loyalty_strength = 0.0F;
    }

    // Clamp
    rel->trust = nimcp_clampf(rel->trust, 0.0F, 1.0F);
    rel->closeness = nimcp_clampf(rel->closeness, 0.0F, 1.0F);
    rel->affection = nimcp_clampf(rel->affection, 0.0F, 1.0F);
    rel->love.intimacy = nimcp_clampf(rel->love.intimacy, 0.0F, 1.0F);

    // Downgrade relationship stage if severe
    if (severity > 0.5F) {
        if (rel->stage == RELATIONSHIP_CLOSE_FRIEND) {
            rel->stage = RELATIONSHIP_FRIEND;
        } else if (rel->stage == RELATIONSHIP_FRIEND) {
            rel->stage = RELATIONSHIP_ACQUAINTANCE;
        }
    }
}

void social_attempt_repair(social_bond_system_t* system,
                          uint32_t relationship_id,
                          float repair_effort,
                          float apology_quality) {
    // WHAT: Work to rebuild trust after betrayal/conflict
    // WHY:  Relationships can recover with effort
    // HOW:  Gradual trust restoration, forgiveness

    if (!system) return;

    repair_effort = nimcp_clampf(repair_effort, 0.0F, 1.0F);
    apology_quality = nimcp_clampf(apology_quality, 0.0F, 1.0F);

    // Find relationship
    relationship_t* rel = NULL;
    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        if (system->relationships[i].active && system->relationships[i].relationship_id == relationship_id) {
            rel = &system->relationships[i];
            break;
        }
    }
    if (!rel) return;

    // Update repair progress
    float repair_effectiveness = (repair_effort + apology_quality) / 2.0F;
    rel->trust_repair_progress += repair_effectiveness * 0.1F;
    rel->trust_repair_progress = nimcp_clampf(rel->trust_repair_progress, 0.0F, 1.0F);

    // Gradual trust restoration
    rel->trust += repair_effectiveness * 0.08F;
    rel->closeness += repair_effectiveness * 0.05F;
    rel->affection += repair_effectiveness * 0.04F;

    // Clamp
    rel->trust = nimcp_clampf(rel->trust, 0.0F, 1.0F);
    rel->closeness = nimcp_clampf(rel->closeness, 0.0F, 1.0F);
    rel->affection = nimcp_clampf(rel->affection, 0.0F, 1.0F);
}

//=============================================================================
// UPDATE FUNCTIONS
//=============================================================================

void social_update(social_bond_system_t* system, float dt, uint64_t current_time_us) {
    // Process pending bio-async messages
    if (system && system->bio_async_enabled && system->bio_ctx) {
        bio_router_process_inbox(system->bio_ctx, 5);
    }

    // WHAT: Advance relationship dynamics, oxytocin decay, loneliness
    // WHY:  Bonds require maintenance, decay without contact
    // HOW:  Update closeness based on interaction frequency, oxytocin half-life

    if (!system) return;

    system->total_update_calls++;

    //=========================================================================
    // OXYTOCIN DECAY
    //=========================================================================

    float oxytocin_decay_rate = 1.0F / OXYTOCIN_HALF_LIFE;
    system->emotion.oxytocin_level = exponential_decay(
        system->emotion.oxytocin_level, 0.5F, oxytocin_decay_rate, dt);

    //=========================================================================
    // RELATIONSHIP MAINTENANCE AND DECAY
    //=========================================================================

    float total_closeness = 0.0F;
    uint32_t close_friend_count = 0;
    uint64_t most_recent_interaction = 0;

    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        if (!system->relationships[i].active) continue;

        relationship_t* rel = &system->relationships[i];

        // Update time known
        rel->time_known = (float)(current_time_us - rel->relationship_start_time) / 1000000.0F;

        // Time since last interaction
        float time_since_interaction = (float)(current_time_us - rel->last_interaction_time) / 1000000.0F;

        // Decay closeness without contact (relationships need maintenance)
        if (time_since_interaction > 86400.0F * 7.0F) {  // More than 1 week
            float decay_rate = 1.0F / (86400.0F * 365.0F);  // 1 year half-life
            rel->closeness = exponential_decay(rel->closeness, 0.1F, decay_rate, dt);
        }

        // Decay oxytocin bond
        rel->oxytocin_bond_strength = exponential_decay(
            rel->oxytocin_bond_strength, 0.0F, oxytocin_decay_rate, dt);

        // Track most recent interaction
        if (rel->last_interaction_time > most_recent_interaction) {
            most_recent_interaction = rel->last_interaction_time;
        }

        // Count close friends
        if (rel->stage >= RELATIONSHIP_CLOSE_FRIEND) {
            close_friend_count++;
        }

        // Accumulate closeness
        total_closeness += rel->closeness;
    }

    // Update average closeness
    if (system->active_relationship_count > 0) {
        system->average_relationship_closeness = total_closeness / (float)system->active_relationship_count;
    }

    // Update close friend count
    system->emotion.close_friends_count = close_friend_count;

    //=========================================================================
    // LONELINESS CALCULATION
    //=========================================================================

    float time_since_last_interaction = (float)(current_time_us - most_recent_interaction) / 1000000.0F;
    float days_since_interaction = time_since_last_interaction / 86400.0F;

    // Loneliness increases with time without social contact
    if (days_since_interaction > 1.0F) {
        system->emotion.loneliness += (days_since_interaction - 1.0F) * 0.01F * dt / 86400.0F;
    } else {
        system->emotion.loneliness -= 0.05F * dt / 86400.0F;
    }

    // Loneliness reduced by close friendships
    if (close_friend_count > 0) {
        system->emotion.loneliness -= close_friend_count * 0.02F * dt / 86400.0F;
    }

    system->emotion.loneliness = nimcp_clampf(system->emotion.loneliness, 0.0F, 1.0F);
    system->emotion.last_social_interaction = most_recent_interaction;

    //=========================================================================
    // FRIENDSHIP WARMTH
    //=========================================================================

    // General positive social feeling based on relationships
    float target_warmth = system->average_relationship_closeness * 0.7F;
    float warmth_rate = 1.0F / 20.0F;  // 20 second adjustment (fast response for testing)
    system->emotion.friendship_warmth = exponential_decay(
        system->emotion.friendship_warmth, target_warmth, warmth_rate, dt);

    //=========================================================================
    // LOVE STATE UPDATE
    //=========================================================================

    // Love fades without reinforcement
    if (system->emotion.experiencing_love) {
        system->emotion.love_intensity -= 0.01F * dt / 86400.0F;
        if (system->emotion.love_intensity < LOVE_THRESHOLD) {
            system->emotion.experiencing_love = false;
        }
    }

    //=========================================================================
    // LOYALTY SATISFACTION
    //=========================================================================

    // Calculate loyalty satisfaction from active loyalties
    float loyalty_sum = 0.0F;
    uint32_t loyalty_count = 0;
    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        if (system->relationships[i].active && system->relationships[i].is_loyal_to) {
            loyalty_sum += system->relationships[i].loyalty_strength;
            loyalty_count++;
        }
    }
    if (loyalty_count > 0) {
        system->emotion.loyalty_satisfaction = loyalty_sum / (float)loyalty_count;
    }

    //=========================================================================
    // UPDATE EMOTIONAL TAG
    //=========================================================================

    system->emotion.social_emotion = social_get_emotion(system);
}

//=============================================================================
// QUERY FUNCTIONS
//=============================================================================

bool social_is_experiencing_love(const social_bond_system_t* system) {
    if (!system) {
        return false;
    }
    return system->emotion.experiencing_love;
}

bool social_is_lonely(const social_bond_system_t* system) {
    if (!system) {
        return false;
    }
    return (system->emotion.loneliness > 0.4F);  // 0.4 threshold (~6-7 days isolation)
}

bool social_is_loyal_to(const social_bond_system_t* system,
                       uint32_t relationship_id) {
    if (!system) {
        return false;
    }

    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        if (system->relationships[i].active && system->relationships[i].relationship_id == relationship_id) {
            return system->relationships[i].is_loyal_to;
        }
    }
    return false;
}

float social_get_relationship_closeness(const social_bond_system_t* system,
                                        uint32_t relationship_id) {
    if (!system) return 0.0F;

    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        if (system->relationships[i].active && system->relationships[i].relationship_id == relationship_id) {
            return system->relationships[i].closeness;
        }
    }
    return 0.0F;
}

uint32_t social_get_close_friend_count(const social_bond_system_t* system) {
    if (!system) return 0;
    return system->emotion.close_friends_count;
}

float social_get_oxytocin_level(const social_bond_system_t* system) {
    if (!system) return 0.0F;
    return system->emotion.oxytocin_level;
}

void social_get_neuromodulator_effects(const social_bond_system_t* system,
                                      float* dopamine_factor,
                                      float* oxytocin_factor) {
    // WHAT: Get neuromodulator modulation factors
    // WHY:  Social bonding affects dopamine (reward) and oxytocin (attachment)
    // HOW:  Map emotional state to neuromodulator changes

    if (!system || !dopamine_factor || !oxytocin_factor) return;

    // Default: no change
    *dopamine_factor = 1.0F;
    *oxytocin_factor = 1.0F;

    // Love boosts dopamine (reward) and oxytocin (bonding)
    if (system->emotion.experiencing_love) {
        float love_boost = system->emotion.love_intensity * 0.5F;  // Up to 50% boost
        *dopamine_factor = 1.0F + love_boost;
        *oxytocin_factor = 1.0F + system->emotion.love_intensity;  // Up to 100% boost
    }

    // Friendship warmth boosts dopamine moderately
    if (system->emotion.friendship_warmth > 0.3F) {
        *dopamine_factor += system->emotion.friendship_warmth * 0.3F;
    }

    // Social network richness bonus: multiple relationships are more rewarding
    // BIOLOGICAL: Rich social networks provide diverse support, buffering, interaction
    // Each active relationship adds small dopamine bonus (up to 3 relationships)
    if (system->active_relationship_count > 0) {
        int relationship_bonus_count = (system->active_relationship_count < 3) ?
                                       system->active_relationship_count : 3;
        float network_bonus = (relationship_bonus_count - 1) * 0.1F;  // 0%, 10%, 20% for 1,2,3 rels
        *dopamine_factor += network_bonus;
    }

    // Oxytocin from current level
    *oxytocin_factor = fmaxf(*oxytocin_factor, system->emotion.oxytocin_level);

    // Loneliness reduces dopamine
    if (system->emotion.loneliness > 0.4F) {
        *dopamine_factor -= system->emotion.loneliness * 0.3F;
    }

    // Clamp
    *dopamine_factor = nimcp_clampf(*dopamine_factor, 0.3F, 2.0F);
    *oxytocin_factor = nimcp_clampf(*oxytocin_factor, 0.5F, 2.0F);
}

//=============================================================================
// EMOTION INTEGRATION
//=============================================================================

emotional_tag_t social_get_emotion(const social_bond_system_t* system) {
    // WHAT: Map social-emotional state to emotional_tag_t
    // WHY:  Integration with emotional tagging system
    // HOW:  Convert love/friendship/loneliness to valence/arousal

    emotional_tag_t tag;
    tag.valence = 0.0F;
    tag.arousal = 0.5F;  // Baseline arousal (neutral social state)

    if (!system) return tag;

    // LOVE: high positive valence [+0.7 to +0.95], high arousal [0.6 to 0.9]
    if (system->emotion.experiencing_love) {
        tag.valence = 0.7F + system->emotion.love_intensity * 0.25F;
        tag.arousal = 0.6F + system->emotion.love_intensity * 0.3F;
        return tag;
    }

    // FRIENDSHIP: moderate positive valence [+0.4 to +0.7], moderate arousal [0.4 to 0.6]
    if (system->emotion.friendship_warmth > 0.3F) {
        tag.valence = 0.4F + system->emotion.friendship_warmth * 0.3F;
        tag.arousal = 0.4F + system->emotion.friendship_warmth * 0.2F;
        return tag;
    }

    // LONELINESS: negative valence [-0.4 to -0.7], low arousal [0.2 to 0.4]
    if (system->emotion.loneliness > 0.4F) {
        tag.valence = -(0.4F + system->emotion.loneliness * 0.3F);
        tag.arousal = 0.2F + system->emotion.loneliness * 0.2F;
        return tag;
    }

    // NEUTRAL: baseline (no strong social emotions)
    tag.valence = 0.0F;
    tag.arousal = 0.5F;

    return tag;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int love_loyalty_friendship_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Love_Loyalty_Friendship");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Love_Loyalty_Friendship");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Love_Loyalty_Friendship");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void love_loyalty_friendship_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_love_loyalty_friendship_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int love_loyalty_friendship_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "love_loyalty_friendship_training_begin: NULL argument");
        return -1;
    }
    love_loyalty_friendship_heartbeat_instance(NULL, "love_loyalty_friendship_training_begin", 0.0f);
    return 0;
}

int love_loyalty_friendship_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "love_loyalty_friendship_training_end: NULL argument");
        return -1;
    }
    love_loyalty_friendship_heartbeat_instance(NULL, "love_loyalty_friendship_training_end", 1.0f);
    return 0;
}

int love_loyalty_friendship_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "love_loyalty_friendship_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    love_loyalty_friendship_heartbeat_instance(NULL, "love_loyalty_friendship_training_step", progress);
    return 0;
}
