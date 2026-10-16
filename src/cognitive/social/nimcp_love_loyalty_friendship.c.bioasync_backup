/**
 * @file nimcp_love_loyalty_friendship.c
 * @brief Implementation of love, loyalty, and friendship system
 */

#include "cognitive/nimcp_love_loyalty_friendship.h"
#include "utils/memory/nimcp_memory.h"
#include <math.h>
#include <string.h>

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

static inline float clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static inline float exponential_decay(float current, float target, float decay_rate, float dt) {
    float decay_factor = expf(-decay_rate * dt);
    return current * decay_factor + target * (1.0f - decay_factor);
}

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

social_bond_system_t* social_bond_system_create(void) {
    // WHAT: Allocate and initialize social bond system
    // WHY:  Central system for love, loyalty, friendship
    // HOW:  Zero-initialize all state, set up default parameters

    social_bond_system_t* system = (social_bond_system_t*)nimcp_calloc(1, sizeof(social_bond_system_t));
    if (!system) return NULL;

    // Initialize all relationships as inactive
    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        system->relationships[i].active = false;
    }

    // Default emotional state
    system->emotion.attachment_style = ATTACHMENT_SECURE;
    system->emotion.attachment_security = 0.7f;
    system->emotion.oxytocin_level = 0.5f;

    // Default personality (moderate values)
    system->extraversion = 0.5f;
    system->agreeableness = 0.6f;
    system->openness_to_experience = 0.5f;

    // Default capacities
    system->love_capacity = 0.8f;
    system->friendship_capacity = 0.8f;
    system->loyalty_capacity = 0.7f;

    // Default social skills
    system->empathy = 0.6f;
    system->perspective_taking = 0.6f;
    system->conflict_resolution = 0.5f;

    // Default: integrate with other systems
    system->integrate_with_theory_of_mind = true;
    system->integrate_with_oxytocin = true;
    system->integrate_with_reward = true;

    return system;
}

void social_bond_system_destroy(social_bond_system_t* system) {
    // WHAT: Free social bond system memory
    // WHY:  Prevent memory leaks
    // HOW:  Free main structure

    if (!system) return;
    nimcp_free(system);
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
    system->emotion.attachment_security = 0.7f;
    system->emotion.oxytocin_level = 0.5f;

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
    system->average_relationship_closeness = 0.0f;
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
            rel->closeness = (initial_stage == RELATIONSHIP_STRANGER) ? 0.0f : 0.2f;
            rel->trust = 0.5f;  // Start with moderate trust
            rel->affection = 0.1f;
            rel->reciprocity = 0.5f;

            // Initialize love components
            rel->love.intimacy = 0.0f;
            rel->love.passion = 0.0f;
            rel->love.commitment = 0.0f;

            // Initialize loyalty
            rel->is_loyal_to = false;
            rel->loyalty_strength = 0.0f;

            // Initialize friendship qualities
            rel->shared_experiences_count = 0.0f;
            rel->vulnerability_shared = 0.0f;
            rel->support_given = 0.0f;
            rel->support_received = 0.0f;

            // Initialize oxytocin bonding
            rel->oxytocin_bond_strength = 0.0f;
            rel->last_interaction_time = current_time_us;

            // Initialize all experiences as inactive
            for (int j = 0; j < SOCIAL_MAX_SHARED_EXPERIENCES; j++) {
                rel->experiences[j].active = false;
            }
            rel->experience_count = 0;
            rel->experience_history_index = 0;

            // Trust violations
            rel->betrayals_experienced = 0;
            rel->trust_repair_progress = 1.0f;

            // Duration
            rel->relationship_start_time = current_time_us;
            rel->time_known = 0.0f;

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
    emotional_intensity = clamp(emotional_intensity, 0.0f, 1.0f);
    valence = clamp(valence, -1.0f, 1.0f);

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
    exp->strengthened_bond = (valence > 0.0f && emotional_intensity > 0.3f);

    rel->experience_count++;
    rel->experience_history_index++;
    rel->shared_experiences_count += 1.0f;

    // Update last interaction time
    rel->last_interaction_time = current_time_us;

    // Update relationship dimensions based on interaction type
    float closeness_change = 0.0f;
    float trust_change = 0.0f;
    float affection_change = 0.0f;

    switch (type) {
        case INTERACTION_CONVERSATION:
            closeness_change = valence * emotional_intensity * 0.05f;
            affection_change = valence * 0.03f;
            break;

        case INTERACTION_SHARED_ACTIVITY:
            closeness_change = valence * emotional_intensity * 0.08f;
            rel->shared_experiences_count += emotional_intensity;
            break;

        case INTERACTION_SUPPORT_GIVEN:
            rel->support_given += emotional_intensity * 0.1f;
            rel->reciprocity += 0.05f;
            trust_change = 0.05f;
            break;

        case INTERACTION_SUPPORT_RECEIVED:
            rel->support_received += emotional_intensity * 0.1f;
            rel->reciprocity -= 0.05f;  // Creates debt to reciprocate
            trust_change = valence * 0.08f;
            closeness_change = valence * 0.06f;
            break;

        case INTERACTION_VULNERABILITY:
            // Handled separately by social_express_vulnerability
            break;

        case INTERACTION_CONFLICT:
            closeness_change = -emotional_intensity * 0.1f;
            trust_change = -emotional_intensity * 0.05f;
            affection_change = -emotional_intensity * 0.08f;
            break;

        case INTERACTION_RECONCILIATION:
            closeness_change = emotional_intensity * 0.15f;
            trust_change = emotional_intensity * 0.1f;
            rel->trust_repair_progress += 0.1f;
            break;

        case INTERACTION_BETRAYAL:
            // Handled separately by social_experience_betrayal
            break;

        case INTERACTION_CELEBRATION:
            closeness_change = emotional_intensity * 0.1f;
            affection_change = emotional_intensity * 0.08f;
            rel->love.intimacy += emotional_intensity * 0.05f;
            break;

        case INTERACTION_COLLABORATION:
            closeness_change = valence * emotional_intensity * 0.07f;
            trust_change = valence * emotional_intensity * 0.06f;
            rel->love.commitment += 0.03f;
            break;
    }

    // Apply changes
    rel->closeness += closeness_change;
    rel->trust += trust_change;
    rel->affection += affection_change;

    // Clamp values
    rel->closeness = clamp(rel->closeness, 0.0f, 1.0f);
    rel->trust = clamp(rel->trust, 0.0f, 1.0f);
    rel->affection = clamp(rel->affection, 0.0f, 1.0f);
    rel->reciprocity = clamp(rel->reciprocity, 0.0f, 1.0f);

    // Update relationship stage based on closeness
    if (rel->closeness >= CLOSE_FRIEND_THRESHOLD && rel->stage < RELATIONSHIP_CLOSE_FRIEND) {
        rel->stage = RELATIONSHIP_CLOSE_FRIEND;
        system->total_friendships_formed++;
    } else if (rel->closeness >= FRIENDSHIP_THRESHOLD && rel->stage < RELATIONSHIP_FRIEND) {
        rel->stage = RELATIONSHIP_FRIEND;
    } else if (rel->closeness > 0.1f && rel->stage == RELATIONSHIP_STRANGER) {
        rel->stage = RELATIONSHIP_ACQUAINTANCE;
    }

    // Oxytocin boost from positive interactions
    if (valence > 0.0f && emotional_intensity > 0.4f) {
        rel->oxytocin_bond_strength += emotional_intensity * 0.1f;
        rel->oxytocin_bond_strength = clamp(rel->oxytocin_bond_strength, 0.0f, 1.0f);

        system->emotion.oxytocin_level += emotional_intensity * 0.15f;
        system->emotion.oxytocin_level = clamp(system->emotion.oxytocin_level, 0.0f, 1.0f);
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

    vulnerability_level = clamp(vulnerability_level, 0.0f, 1.0f);

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
    rel->vulnerability_shared += vulnerability_level * 0.1f;
    rel->vulnerability_shared = clamp(rel->vulnerability_shared, 0.0f, 1.0f);

    if (received_well) {
        // Vulnerability accepted → deepens bond
        float intimacy_increase = vulnerability_level * 0.15f;
        rel->love.intimacy += intimacy_increase;
        rel->closeness += vulnerability_level * 0.12f;
        rel->trust += vulnerability_level * 0.1f;

        // Major intimacy boost
        rel->love.intimacy = clamp(rel->love.intimacy, 0.0f, 1.0f);
        rel->closeness = clamp(rel->closeness, 0.0f, 1.0f);
        rel->trust = clamp(rel->trust, 0.0f, 1.0f);
    } else {
        // Vulnerability rejected → damages bond
        rel->closeness -= vulnerability_level * 0.2f;
        rel->trust -= vulnerability_level * 0.15f;
        rel->closeness = clamp(rel->closeness, 0.0f, 1.0f);
        rel->trust = clamp(rel->trust, 0.0f, 1.0f);
    }
}

void social_provide_support(social_bond_system_t* system,
                           uint32_t relationship_id,
                           float support_quality) {
    // WHAT: Help, comfort, or assist in time of need
    // WHY:  Support builds reciprocity and strengthens bonds
    // HOW:  Update support_given, reciprocity, closeness

    if (!system) return;

    support_quality = clamp(support_quality, 0.0f, 1.0f);

    // Find relationship
    relationship_t* rel = NULL;
    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        if (system->relationships[i].active && system->relationships[i].relationship_id == relationship_id) {
            rel = &system->relationships[i];
            break;
        }
    }
    if (!rel) return;

    rel->support_given += support_quality * 0.1f;
    rel->reciprocity += support_quality * 0.08f;
    rel->closeness += support_quality * 0.06f;
    rel->trust += support_quality * 0.05f;

    // Clamp
    rel->support_given = clamp(rel->support_given, 0.0f, 2.0f);  // Can accumulate
    rel->reciprocity = clamp(rel->reciprocity, 0.0f, 1.0f);
    rel->closeness = clamp(rel->closeness, 0.0f, 1.0f);
    rel->trust = clamp(rel->trust, 0.0f, 1.0f);
}

void social_receive_support(social_bond_system_t* system,
                           uint32_t relationship_id,
                           float support_quality) {
    // WHAT: Be helped by another in time of need
    // WHY:  Receiving support creates gratitude and reciprocity
    // HOW:  Update support_received, trust, closeness

    if (!system) return;

    support_quality = clamp(support_quality, 0.0f, 1.0f);

    // Find relationship
    relationship_t* rel = NULL;
    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        if (system->relationships[i].active && system->relationships[i].relationship_id == relationship_id) {
            rel = &system->relationships[i];
            break;
        }
    }
    if (!rel) return;

    rel->support_received += support_quality * 0.1f;
    rel->reciprocity -= support_quality * 0.05f;  // Creates obligation
    rel->closeness += support_quality * 0.08f;
    rel->trust += support_quality * 0.1f;
    rel->affection += support_quality * 0.06f;

    // Clamp
    rel->support_received = clamp(rel->support_received, 0.0f, 2.0f);
    rel->reciprocity = clamp(rel->reciprocity, 0.0f, 1.0f);
    rel->closeness = clamp(rel->closeness, 0.0f, 1.0f);
    rel->trust = clamp(rel->trust, 0.0f, 1.0f);
    rel->affection = clamp(rel->affection, 0.0f, 1.0f);
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

    strength = clamp(strength, 0.0f, 1.0f);

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
    rel->love.commitment += strength * 0.3f;
    rel->love.commitment = clamp(rel->love.commitment, 0.0f, 1.0f);

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

    test_difficulty = clamp(test_difficulty, 0.0f, 1.0f);

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
        rel->loyalty_strength += test_difficulty * 0.1f;
        rel->loyalty_strength = clamp(rel->loyalty_strength, 0.0f, 1.0f);
        rel->love.commitment += test_difficulty * 0.08f;
        rel->love.commitment = clamp(rel->love.commitment, 0.0f, 1.0f);
    } else {
        // Failed test → damages loyalty
        rel->loyalty_tests_failed++;
        rel->loyalty_strength -= test_difficulty * 0.3f;
        if (rel->loyalty_strength < LOYALTY_THRESHOLD) {
            rel->is_loyal_to = false;
        }
        rel->trust -= test_difficulty * 0.2f;
        rel->trust = clamp(rel->trust, 0.0f, 1.0f);

        // Broken loyalty also damages relationship closeness
        rel->closeness -= test_difficulty * 0.15f;
        rel->closeness = clamp(rel->closeness, 0.0f, 1.0f);
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

    intensity = clamp(intensity, 0.0f, 1.0f);

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
            rel->love.intimacy += intensity * 0.1f;
            rel->love.passion += intensity * 0.15f;
            rel->love.commitment += intensity * 0.08f;
            rel->stage = RELATIONSHIP_ROMANTIC;
            break;

        case LOVE_TYPE_COMPANIONATE:
            rel->love.intimacy += intensity * 0.12f;
            rel->love.passion += intensity * 0.02f;
            rel->love.commitment += intensity * 0.12f;
            break;

        case LOVE_TYPE_FAMILIAL:
            rel->love.intimacy += intensity * 0.08f;
            rel->love.commitment += intensity * 0.15f;
            rel->stage = RELATIONSHIP_FAMILY;
            break;

        case LOVE_TYPE_PLATONIC:
            rel->love.intimacy += intensity * 0.1f;
            rel->love.commitment += intensity * 0.08f;
            break;

        case LOVE_TYPE_COMPASSIONATE:
            rel->affection += intensity * 0.1f;
            break;

        case LOVE_TYPE_SELF_LOVE:
            // Self-love handled differently
            break;
    }

    // Clamp love components
    rel->love.intimacy = clamp(rel->love.intimacy, 0.0f, 1.0f);
    rel->love.passion = clamp(rel->love.passion, 0.0f, 1.0f);
    rel->love.commitment = clamp(rel->love.commitment, 0.0f, 1.0f);

    // Update affection and closeness
    rel->affection += intensity * 0.1f;
    rel->closeness += intensity * 0.08f;
    rel->affection = clamp(rel->affection, 0.0f, 1.0f);
    rel->closeness = clamp(rel->closeness, 0.0f, 1.0f);

    // Update system-level love state
    system->emotion.experiencing_love = true;
    system->emotion.love_intensity = intensity;
    system->emotion.love_type_felt = love_type;
    system->total_loves_experienced++;

    // Oxytocin boost
    system->emotion.oxytocin_level += intensity * 0.2f;
    system->emotion.oxytocin_level = clamp(system->emotion.oxytocin_level, 0.0f, 1.0f);
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

    severity = clamp(severity, 0.0f, 1.0f);

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
    rel->trust_repair_progress = 0.0f;

    // Damage relationship dimensions
    rel->trust -= severity * 0.6f;
    rel->closeness -= severity * 0.4f;
    rel->affection -= severity * 0.5f;
    rel->love.intimacy -= severity * 0.4f;

    // Break loyalty if severe
    if (severity > 0.7f && rel->is_loyal_to) {
        rel->is_loyal_to = false;
        rel->loyalty_strength = 0.0f;
    }

    // Clamp
    rel->trust = clamp(rel->trust, 0.0f, 1.0f);
    rel->closeness = clamp(rel->closeness, 0.0f, 1.0f);
    rel->affection = clamp(rel->affection, 0.0f, 1.0f);
    rel->love.intimacy = clamp(rel->love.intimacy, 0.0f, 1.0f);

    // Downgrade relationship stage if severe
    if (severity > 0.5f) {
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

    repair_effort = clamp(repair_effort, 0.0f, 1.0f);
    apology_quality = clamp(apology_quality, 0.0f, 1.0f);

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
    float repair_effectiveness = (repair_effort + apology_quality) / 2.0f;
    rel->trust_repair_progress += repair_effectiveness * 0.1f;
    rel->trust_repair_progress = clamp(rel->trust_repair_progress, 0.0f, 1.0f);

    // Gradual trust restoration
    rel->trust += repair_effectiveness * 0.08f;
    rel->closeness += repair_effectiveness * 0.05f;
    rel->affection += repair_effectiveness * 0.04f;

    // Clamp
    rel->trust = clamp(rel->trust, 0.0f, 1.0f);
    rel->closeness = clamp(rel->closeness, 0.0f, 1.0f);
    rel->affection = clamp(rel->affection, 0.0f, 1.0f);
}

//=============================================================================
// UPDATE FUNCTIONS
//=============================================================================

void social_update(social_bond_system_t* system, float dt, uint64_t current_time_us) {
    // WHAT: Advance relationship dynamics, oxytocin decay, loneliness
    // WHY:  Bonds require maintenance, decay without contact
    // HOW:  Update closeness based on interaction frequency, oxytocin half-life

    if (!system) return;

    system->total_update_calls++;

    //=========================================================================
    // OXYTOCIN DECAY
    //=========================================================================

    float oxytocin_decay_rate = 1.0f / OXYTOCIN_HALF_LIFE;
    system->emotion.oxytocin_level = exponential_decay(
        system->emotion.oxytocin_level, 0.5f, oxytocin_decay_rate, dt);

    //=========================================================================
    // RELATIONSHIP MAINTENANCE AND DECAY
    //=========================================================================

    float total_closeness = 0.0f;
    uint32_t close_friend_count = 0;
    uint64_t most_recent_interaction = 0;

    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        if (!system->relationships[i].active) continue;

        relationship_t* rel = &system->relationships[i];

        // Update time known
        rel->time_known = (float)(current_time_us - rel->relationship_start_time) / 1000000.0f;

        // Time since last interaction
        float time_since_interaction = (float)(current_time_us - rel->last_interaction_time) / 1000000.0f;

        // Decay closeness without contact (relationships need maintenance)
        if (time_since_interaction > 86400.0f * 7.0f) {  // More than 1 week
            float decay_rate = 1.0f / (86400.0f * 365.0f);  // 1 year half-life
            rel->closeness = exponential_decay(rel->closeness, 0.1f, decay_rate, dt);
        }

        // Decay oxytocin bond
        rel->oxytocin_bond_strength = exponential_decay(
            rel->oxytocin_bond_strength, 0.0f, oxytocin_decay_rate, dt);

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

    float time_since_last_interaction = (float)(current_time_us - most_recent_interaction) / 1000000.0f;
    float days_since_interaction = time_since_last_interaction / 86400.0f;

    // Loneliness increases with time without social contact
    if (days_since_interaction > 1.0f) {
        system->emotion.loneliness += (days_since_interaction - 1.0f) * 0.01f * dt / 86400.0f;
    } else {
        system->emotion.loneliness -= 0.05f * dt / 86400.0f;
    }

    // Loneliness reduced by close friendships
    if (close_friend_count > 0) {
        system->emotion.loneliness -= close_friend_count * 0.02f * dt / 86400.0f;
    }

    system->emotion.loneliness = clamp(system->emotion.loneliness, 0.0f, 1.0f);
    system->emotion.last_social_interaction = most_recent_interaction;

    //=========================================================================
    // FRIENDSHIP WARMTH
    //=========================================================================

    // General positive social feeling based on relationships
    float target_warmth = system->average_relationship_closeness * 0.7f;
    float warmth_rate = 1.0f / 20.0f;  // 20 second adjustment (fast response for testing)
    system->emotion.friendship_warmth = exponential_decay(
        system->emotion.friendship_warmth, target_warmth, warmth_rate, dt);

    //=========================================================================
    // LOVE STATE UPDATE
    //=========================================================================

    // Love fades without reinforcement
    if (system->emotion.experiencing_love) {
        system->emotion.love_intensity -= 0.01f * dt / 86400.0f;
        if (system->emotion.love_intensity < LOVE_THRESHOLD) {
            system->emotion.experiencing_love = false;
        }
    }

    //=========================================================================
    // LOYALTY SATISFACTION
    //=========================================================================

    // Calculate loyalty satisfaction from active loyalties
    float loyalty_sum = 0.0f;
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
    if (!system) return false;
    return system->emotion.experiencing_love;
}

bool social_is_lonely(const social_bond_system_t* system) {
    if (!system) return false;
    return (system->emotion.loneliness > 0.4f);  // 0.4 threshold (~6-7 days isolation)
}

bool social_is_loyal_to(const social_bond_system_t* system,
                       uint32_t relationship_id) {
    if (!system) return false;

    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        if (system->relationships[i].active && system->relationships[i].relationship_id == relationship_id) {
            return system->relationships[i].is_loyal_to;
        }
    }
    return false;
}

float social_get_relationship_closeness(const social_bond_system_t* system,
                                        uint32_t relationship_id) {
    if (!system) return 0.0f;

    for (int i = 0; i < SOCIAL_MAX_RELATIONSHIPS; i++) {
        if (system->relationships[i].active && system->relationships[i].relationship_id == relationship_id) {
            return system->relationships[i].closeness;
        }
    }
    return 0.0f;
}

uint32_t social_get_close_friend_count(const social_bond_system_t* system) {
    if (!system) return 0;
    return system->emotion.close_friends_count;
}

float social_get_oxytocin_level(const social_bond_system_t* system) {
    if (!system) return 0.0f;
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
    *dopamine_factor = 1.0f;
    *oxytocin_factor = 1.0f;

    // Love boosts dopamine (reward) and oxytocin (bonding)
    if (system->emotion.experiencing_love) {
        float love_boost = system->emotion.love_intensity * 0.5f;  // Up to 50% boost
        *dopamine_factor = 1.0f + love_boost;
        *oxytocin_factor = 1.0f + system->emotion.love_intensity;  // Up to 100% boost
    }

    // Friendship warmth boosts dopamine moderately
    if (system->emotion.friendship_warmth > 0.3f) {
        *dopamine_factor += system->emotion.friendship_warmth * 0.3f;
    }

    // Social network richness bonus: multiple relationships are more rewarding
    // BIOLOGICAL: Rich social networks provide diverse support, buffering, interaction
    // Each active relationship adds small dopamine bonus (up to 3 relationships)
    if (system->active_relationship_count > 0) {
        int relationship_bonus_count = (system->active_relationship_count < 3) ?
                                       system->active_relationship_count : 3;
        float network_bonus = (relationship_bonus_count - 1) * 0.1f;  // 0%, 10%, 20% for 1,2,3 rels
        *dopamine_factor += network_bonus;
    }

    // Oxytocin from current level
    *oxytocin_factor = fmaxf(*oxytocin_factor, system->emotion.oxytocin_level);

    // Loneliness reduces dopamine
    if (system->emotion.loneliness > 0.4f) {
        *dopamine_factor -= system->emotion.loneliness * 0.3f;
    }

    // Clamp
    *dopamine_factor = clamp(*dopamine_factor, 0.3f, 2.0f);
    *oxytocin_factor = clamp(*oxytocin_factor, 0.5f, 2.0f);
}

//=============================================================================
// EMOTION INTEGRATION
//=============================================================================

emotional_tag_t social_get_emotion(const social_bond_system_t* system) {
    // WHAT: Map social-emotional state to emotional_tag_t
    // WHY:  Integration with emotional tagging system
    // HOW:  Convert love/friendship/loneliness to valence/arousal

    emotional_tag_t tag;
    tag.valence = 0.0f;
    tag.arousal = 0.5f;  // Baseline arousal (neutral social state)

    if (!system) return tag;

    // LOVE: high positive valence [+0.7 to +0.95], high arousal [0.6 to 0.9]
    if (system->emotion.experiencing_love) {
        tag.valence = 0.7f + system->emotion.love_intensity * 0.25f;
        tag.arousal = 0.6f + system->emotion.love_intensity * 0.3f;
        return tag;
    }

    // FRIENDSHIP: moderate positive valence [+0.4 to +0.7], moderate arousal [0.4 to 0.6]
    if (system->emotion.friendship_warmth > 0.3f) {
        tag.valence = 0.4f + system->emotion.friendship_warmth * 0.3f;
        tag.arousal = 0.4f + system->emotion.friendship_warmth * 0.2f;
        return tag;
    }

    // LONELINESS: negative valence [-0.4 to -0.7], low arousal [0.2 to 0.4]
    if (system->emotion.loneliness > 0.4f) {
        tag.valence = -(0.4f + system->emotion.loneliness * 0.3f);
        tag.arousal = 0.2f + system->emotion.loneliness * 0.2f;
        return tag;
    }

    // NEUTRAL: baseline (no strong social emotions)
    tag.valence = 0.0f;
    tag.arousal = 0.5f;

    return tag;
}
