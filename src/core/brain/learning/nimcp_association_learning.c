//=============================================================================
// nimcp_association_learning.c - Association Learning Implementation
//=============================================================================

#include "core/brain/learning/nimcp_association_learning.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "core_association_learning"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(association_learning, MESH_ADAPTER_CATEGORY_COGNITIVE)


#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "constants/nimcp_buffer_constants.h"

//=============================================================================
// Association Storage (Internal)
//=============================================================================

#define MAX_ASSOCIATIONS 1024

typedef struct {
    char antecedent[NIMCP_ERROR_BUFFER_SIZE];
    char consequent[NIMCP_ERROR_BUFFER_SIZE];
    float strength;
    association_stats_t stats;
} association_entry_t;

typedef struct {
    association_entry_t* entries;
    uint32_t count;
    uint32_t capacity;
} association_store_t;

// Global association store (in production, this would be per-brain)
static association_store_t g_associations = {NULL, 0, 0};

static void init_association_store(void) {
    if (!g_associations.entries) {
        g_associations.entries = (association_entry_t*)nimcp_calloc(MAX_ASSOCIATIONS,
                                                              sizeof(association_entry_t));
        g_associations.capacity = MAX_ASSOCIATIONS;
        g_associations.count = 0;
    }
}

static association_entry_t* find_association(const char* A, const char* B) {
    init_association_store();

    for (uint32_t i = 0; i < g_associations.count; i++) {
        if (strcmp(g_associations.entries[i].antecedent, A) == 0 &&
            strcmp(g_associations.entries[i].consequent, B) == 0) {
            return &g_associations.entries[i];
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_association: validation failed");
    return NULL;
}

static association_entry_t* create_association(const char* A, const char* B) {
    init_association_store();

    if (g_associations.count >= g_associations.capacity) {
        LOG_ERROR("association_learning: Association store full");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_association: capacity exceeded");
        return NULL;
    }

    association_entry_t* entry = &g_associations.entries[g_associations.count++];
    strncpy(entry->antecedent, A, sizeof(entry->antecedent) - 1);
    strncpy(entry->consequent, B, sizeof(entry->consequent) - 1);
    entry->strength = 0.0F;
    memset(&entry->stats, 0, sizeof(entry->stats));

    return entry;
}

//=============================================================================
// Association Learning Implementation
//=============================================================================

bool brain_learn_association(brain_t brain, const char* A, const char* B,
                              uint32_t cooccurrence_count) {
    if (!brain || !A || !B) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "brain_learn_association: invalid parameters");
        return false;
    }

    association_entry_t* entry = find_association(A, B);
    if (!entry) {
        entry = create_association(A, B);
        if (!entry) {

                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                    "if: entry is NULL");

                return false;

            }
    }

    // Update statistics
    entry->stats.AB_count += cooccurrence_count;
    entry->stats.total_count += cooccurrence_count;

    // Compute confidence P(B|A)
    float confidence = compute_association_confidence(A, B, &entry->stats);

    // Update strength (exponential moving average)
    // Apply learning rate update for each cooccurrence to build strength faster
    const float alpha = 0.1F; // Learning rate
    for (uint32_t i = 0; i < cooccurrence_count; i++) {
        entry->strength = alpha * confidence + (1.0F - alpha) * entry->strength;
    }

    LOG_DEBUG("association_learning: Learned association: %s → %s (strength: %.2f, confidence: %.2f, count: %u)",
             A, B, entry->strength, confidence, cooccurrence_count);

    return true;
}

float compute_association_confidence(const char* A, const char* B,
                                     const association_stats_t* stats) {
    if (!stats || stats->total_count == 0) {
        return 0.0F;
    }

    // P(B|A) = count(A,B) / count(A)
    // Approximation: use AB_count / total_count
    float confidence = (float)stats->AB_count / (float)stats->total_count;

    // Clamp to [0.0, 1.0]
    if (confidence < 0.0F) confidence = 0.0F;
    if (confidence > 1.0F) confidence = 1.0F;

    return confidence;
}

float update_association_strength(brain_t brain, const char* A, const char* B,
                                   float outcome) {
    if (!brain || !A || !B) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "update_association_strength: invalid parameters");
        return -1.0F;
    }

    association_entry_t* entry = find_association(A, B);
    if (!entry) {
        entry = create_association(A, B);
        if (!entry) return -1.0F;
    }

    // Higher learning rate for explicit reinforcement/rehearsal than passive learning
    const float learning_rate = 0.2F;  // Was 0.1f - increased to make rehearsal more effective

    // Reinforcement-based update
    if (outcome > 0.0F) {
        // Positive reinforcement: approach 1.0
        entry->strength += learning_rate * outcome * (1.0F - entry->strength);
    } else if (outcome < 0.0F) {
        // Negative reinforcement: approach 0.0
        entry->strength += learning_rate * outcome * entry->strength;
    }

    // Clamp to [0.0, 1.0]
    if (entry->strength < 0.0F) entry->strength = 0.0F;
    if (entry->strength > 1.0F) entry->strength = 1.0F;

    LOG_DEBUG("association_learning: Updated association: %s → %s (strength: %.2f, outcome: %.2f)",
             A, B, entry->strength, outcome);

    return entry->strength;
}

float get_association_strength(brain_t brain, const char* A, const char* B) {
    if (!brain || !A || !B) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "get_association_strength: invalid parameters");
        return -1.0F;
    }

    association_entry_t* entry = find_association(A, B);
    if (!entry) {
        return -1.0F;
    }

    return entry->strength;
}

uint32_t decay_all_associations(brain_t brain, float decay_factor) {
    if (!brain || decay_factor < 0.0F || decay_factor > 1.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "decay_all_associations: invalid parameters");

            return 0;
    }

    init_association_store();

    for (uint32_t i = 0; i < g_associations.count; i++) {
        g_associations.entries[i].strength *= decay_factor;
    }

    LOG_DEBUG("association_learning: Decayed %u associations (factor: %.2f)",
             g_associations.count, decay_factor);

    return g_associations.count;
}
