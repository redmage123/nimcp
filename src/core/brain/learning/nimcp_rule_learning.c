//=============================================================================
// nimcp_rule_learning.c - Inductive Rule Learning Implementation
//=============================================================================

#include "core/brain/learning/nimcp_rule_learning.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "core_rule_learning"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(rule_learning, MESH_ADAPTER_CATEGORY_COGNITIVE)


#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "constants/nimcp_buffer_constants.h"

//=============================================================================
// Rule Learning Implementation
//=============================================================================

int brain_learn_rule_from_examples(brain_t brain, const rule_example_t* examples,
                                     const char** labels, uint32_t count) {
    if (!brain || !examples || !labels || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "brain_learn_rule_from_examples: invalid parameters");
        return -1;
    }

    int rules_learned = 0;

    // Group examples by label
    for (uint32_t i = 0; i < count; i++) {
        const char* current_label = labels[i];

        // Find all examples with this label
        rule_example_t* group = (rule_example_t*)nimcp_malloc(count * sizeof(rule_example_t));
        uint32_t group_size = 0;

        for (uint32_t j = 0; j < count; j++) {
            if (strcmp(labels[j], current_label) == 0) {
                group[group_size++] = examples[j];
            }
        }

        // Extract pattern from grouped examples
        char rule_str[NIMCP_ERROR_BUFFER_LARGE];
        if (extract_rule_pattern(group, group_size, current_label, rule_str, sizeof(rule_str))) {
            // Compute confidence
            float confidence = compute_rule_confidence(group_size, count);

            // Add to KB
            if (add_learned_rule_to_kb(brain, rule_str, confidence)) {
                rules_learned++;
                LOG_INFO("rule_learning: Learned rule: %s (confidence: %.2f)",
                         rule_str, confidence);
            }
        }

        nimcp_free(group);
    }

    return rules_learned;
}

bool extract_rule_pattern(const rule_example_t* examples, uint32_t count,
                          const char* label, char* rule_out, size_t rule_size) {
    if (!examples || count == 0 || !label || !rule_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "extract_rule_pattern: invalid parameters");

            return false;
    }

    // Find common features (threshold: present in >80% of examples)
    uint32_t num_features = examples[0].num_features;
    bool* common_features = (bool*)nimcp_calloc(num_features, sizeof(bool));

    for (uint32_t f = 0; f < num_features; f++) {
        uint32_t present_count = 0;

        // Count how many examples have this feature active
        for (uint32_t i = 0; i < count; i++) {
            if (examples[i].features[f] > 0.5F) {
                present_count++;
            }
        }

        // Feature is common if present in >80% of examples
        if (present_count >= (count * 4) / 5) {
            common_features[f] = true;
        }
    }

    // Build rule string
    int offset = snprintf(rule_out, rule_size, "IF ");
    // SECURITY FIX: Clamp offset to prevent buffer overflow (snprintf returns would-be length, not actual)
    if (offset >= (int)rule_size) offset = rule_size - 1;
    bool first = true;

    for (uint32_t f = 0; f < num_features; f++) {
        if (common_features[f]) {
            if (!first) {
                int written = snprintf(rule_out + offset, rule_size - offset, " AND ");
                offset += written;
                if (offset >= (int)rule_size) offset = rule_size - 1;  // Clamp to prevent overflow
            }
            int written = snprintf(rule_out + offset, rule_size - offset, "feature_%u", f);
            offset += written;
            if (offset >= (int)rule_size) offset = rule_size - 1;  // Clamp to prevent overflow
            first = false;
        }
    }

    if (first) {
        // No common features found
        nimcp_free(common_features);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rule_learning_mesh_unregister: validation failed");
        return false;
    }

    snprintf(rule_out + offset, rule_size - offset, " THEN %s", label);
    nimcp_free(common_features);

    return true;
}

bool add_learned_rule_to_kb(brain_t brain, const char* rule, float confidence) {
    if (!brain || !rule) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "add_learned_rule_to_kb: invalid parameters");

            return false;
    }

    // In a full implementation, this would integrate with the KB module
    // For now, we log the rule
    LOG_INFO("rule_learning: Adding rule to KB: %s (confidence: %.2f)",
             rule, confidence);

    // TODO: Integration with knowledge base module
    // kb_add_rule(b->knowledge_base, rule, confidence);

    return true;
}

float compute_rule_confidence(uint32_t support_count, uint32_t total_count) {
    if (total_count == 0) {
        return 0.0F;
    }

    // Laplace smoothing
    const float alpha = 1.0F;
    const float beta = 2.0F;

    float confidence = (float)(support_count + alpha) / (float)(total_count + beta);

    // Clamp to [0.0, 1.0]
    if (confidence < 0.0F) confidence = 0.0F;
    if (confidence > 1.0F) confidence = 1.0F;

    return confidence;
}
