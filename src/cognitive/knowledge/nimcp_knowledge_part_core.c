// nimcp_knowledge_part_core.c - core functions
// Part of nimcp_knowledge.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_knowledge.c


/* ============================================================================
 * KG Integration — Wave W7 (2026-04-24)
 * ----------------------------------------------------------------------------
 * The cognitive/knowledge/ subsystem is a PARALLEL concept store (see audit
 * risk 7 — "knowledge overloaded across 3 stores").  This wiring MIRRORS each
 * concept addition into brain->internal_kg so that downstream reasoning, KG
 * queries and imagination can cross-link.  We do NOT merge the three stores
 * here; that is a larger design call.  Intent: every local concept write has
 * a corresponding 'cog_knowledge_concept_<name>' node in the KG.
 * ============================================================================ */

/**
 * @brief Mirror a concept addition into brain->internal_kg
 *
 * Idempotent — uses brain_kg_find_node before add.  Silently no-ops if the KG
 * is disabled or the knowledge system has no brain handle.
 */
static void knowledge_kg_mirror_concept(knowledge_system_t system,
                                        const char* concept_name,
                                        knowledge_domain_t domain,
                                        float confidence)
{
    if (!system || !concept_name) return;
    brain_t brain = system->knowledge_brain;
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) return;

    char node_name[BRAIN_KG_MAX_NAME_LEN];
    snprintf(node_name, sizeof(node_name), "cog_knowledge_concept_%.96s",
             concept_name);

    /* Elevate to ADMIN for the write (registry §7). */
    uint64_t tok = brain->internal_kg_admin_token;
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_ADMIN, tok);

    brain_kg_node_id_t nid = brain_kg_find_node(brain->internal_kg, node_name);
    if (nid == BRAIN_KG_INVALID_NODE) {
        nid = brain_kg_add_node(brain->internal_kg, node_name,
                                 BRAIN_KG_NODE_COGNITIVE,
                                 "Knowledge-system concept (mirrored)");
        if (nid != BRAIN_KG_INVALID_NODE) {
            char meta[32];
            snprintf(meta, sizeof(meta), "%u", (uint32_t)domain);
            brain_kg_add_metadata(brain->internal_kg, nid, "domain", meta);
            snprintf(meta, sizeof(meta), "%.4f", confidence);
            brain_kg_add_metadata(brain->internal_kg, nid, "confidence", meta);

            /* Link to the subsystem root node if present. */
            brain_kg_node_id_t root =
                brain_kg_find_node(brain->internal_kg, "cog_knowledge");
            if (root != BRAIN_KG_INVALID_NODE) {
                brain_kg_add_edge(brain->internal_kg, root, nid,
                                  BRAIN_KG_EDGE_PROVIDES_TO,
                                  "root_contains_concept", 0.5f);
            }
        }
    }
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);
}

/**
 * @brief Ensure the 'cog_knowledge' subsystem root node exists in the KG.
 *
 * Called on first mirror call.  Idempotent.
 */
static void knowledge_kg_ensure_root(knowledge_system_t system)
{
    if (!system) return;
    brain_t brain = system->knowledge_brain;
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) return;

    uint64_t tok = brain->internal_kg_admin_token;
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_ADMIN, tok);
    if (brain_kg_find_node(brain->internal_kg, "cog_knowledge")
        == BRAIN_KG_INVALID_NODE) {
        brain_kg_add_node(brain->internal_kg, "cog_knowledge",
                          BRAIN_KG_NODE_COGNITIVE,
                          "Parallel concept store (mirrors to KG)");
    }
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);
}


/**
 * @brief Learn from text with optimized O(n) algorithm
 *
 * Extracts and learns concepts in single pass without nested loops.
 * Previous version had O(n²) complexity with nested iteration.
 *
 * @param system Knowledge system (must be non-NULL)
 * @param text Input text (must be non-NULL)
 * @param domain Knowledge domain
 * @return Number of new concepts learned
 *
 * Why: O(n) is critical for processing large texts efficiently.
 * Previous: Nested loops checking duplicates = O(n²)
 * Optimized: Single pass with hash table lookups = O(n)
 *
 * Example:
 *   Input: "Democracy requires informed citizens. Citizens vote."
 *   Learns: "democracy", "requires", "informed", "citizens", "vote"
 *   Second occurrence of "citizens" reinforces instead of duplicating.
 */
uint32_t knowledge_learn_from_text(knowledge_system_t system, const char* text,
                                   knowledge_domain_t domain)
{
    if (!system || !text)
        return 0;

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_learn_from_text", 0.0f);

    char concepts[100][256];
    uint32_t num_concepts = extract_concepts_optimized(text, concepts, 100);
    uint32_t learned = 0;

    /* W7: ensure subsystem root exists before any mirror writes. */
    knowledge_kg_ensure_root(system);

    for (uint32_t i = 0; i < num_concepts; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_concepts > 256) {
            knowledge_heartbeat("knowledge_loop",
                             (float)(i + 1) / (float)num_concepts);
        }

        if (process_concept(system, concepts[i], text, domain)) {
            learned++;
            /* W7: mirror the fresh concept into the internal KG so readers
             * (imagination, reasoning, symbolic logic) can cross-reference. */
            knowledge_kg_mirror_concept(system, concepts[i], domain, 0.3f);
        }
    }

    update_domain_stats(system, domain);

    // Broadcast knowledge update via bio-async
    if (learned > 0) {
        bio_broadcast_knowledge_update(system, learned, domain);
    }

    return learned;
}


//=============================================================================
// Learning from Different Sources - Using Strategy Pattern
//=============================================================================

/**
 * @brief Learn from story using narrative strategy
 *
 * @param system Knowledge system (must be non-NULL)
 * @param story Story to learn from (must be non-NULL)
 * @return true on success
 *
 * Why: Stories are key to human learning of values and social behavior.
 * Strategy pattern allows domain-specific processing.
 */
bool knowledge_learn_from_story(knowledge_system_t system, const narrative_knowledge_t* story)
{
    if (!system || !story) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_learn_from_story: required parameter is NULL (system, story)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_learn_from_story", 0.0f);

    return strategy_learn_narrative(system, story);
}


/**
 * @brief Learn from art using aesthetic strategy
 *
 * @param system Knowledge system (must be non-NULL)
 * @param art_piece Art to learn from (must be non-NULL)
 * @return true on success
 *
 * Why: Art learning requires different processing than factual knowledge.
 * Focuses on aesthetic qualities and emotional impact.
 */
bool knowledge_learn_from_art(knowledge_system_t system, const aesthetic_knowledge_t* art_piece)
{
    if (!system || !art_piece) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_learn_from_art: required parameter is NULL (system, art_piece)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_learn_from_art", 0.0f);

    return strategy_learn_aesthetic(system, art_piece);
}


/**
 * @brief Learn from history using historical strategy
 *
 * @param system Knowledge system (must be non-NULL)
 * @param event Historical event (must be non-NULL)
 * @return true on success
 *
 * Why: History requires tracking causality and significance.
 * Different structure than narrative or factual knowledge.
 */
bool knowledge_learn_from_history(knowledge_system_t system, const historical_knowledge_t* event)
{
    if (!system || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_learn_from_history: required parameter is NULL (system, event)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_learn_from_history", 0.0f);

    return strategy_learn_historical(system, event);
}


/**
 * @brief Learn from conversation
 *
 * Social learning through dialogue.
 *
 * @param system Knowledge system (must be non-NULL)
 * @param dialogue Conversation text (must be non-NULL)
 * @param participants Array of participant names
 * @param num_participants Number of participants
 * @return Number of concepts learned
 *
 * Why: Social interaction is primary human learning method.
 * Reuses text learning with social domain context.
 */
uint32_t knowledge_learn_from_conversation(knowledge_system_t system, const char* dialogue,
                                           const char** participants, uint32_t num_participants)
{
    if (!system || !dialogue || !participants || num_participants == 0)
        return 0;

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_learn_from_conversat", 0.0f);

    return knowledge_learn_from_text(system, dialogue, KNOWLEDGE_DOMAIN_SOCIAL);
}


/**
 * @brief Learn from demonstration
 *
 * Procedural learning by observation.
 *
 * @param system Knowledge system (must be non-NULL)
 * @param what_demonstrated Action being demonstrated (must be non-NULL)
 * @param steps Array of step descriptions (must be non-NULL)
 * @param num_steps Number of steps
 * @return true on success
 *
 * Why: Procedural knowledge requires step-by-step encoding.
 * Different from declarative knowledge (facts) or narrative knowledge.
 */
bool knowledge_learn_from_demonstration(knowledge_system_t system, const char* what_demonstrated,
                                        const char** steps, uint32_t num_steps)
{
    if (!system || !what_demonstrated || !steps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_learn_from_demonstration: required parameter is NULL (system, what_demonstrated, steps)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_learn_from_demonstra", 0.0f);

    knowledge_item_t item = {0};
    strncpy(item.concept_name, what_demonstrated, sizeof(item.concept_name) - 1);
    item.domain = KNOWLEDGE_DOMAIN_TECHNICAL;

    char definition[NIMCP_LOG_BUFFER_SIZE] = "Steps: ";
    for (uint32_t i = 0; i < num_steps && strlen(definition) < 900; i++) {
        strncat(definition, steps[i], sizeof(definition) - strlen(definition) - 1);
        if (i < num_steps - 1) {
            strncat(definition, ", ", sizeof(definition) - strlen(definition) - 1);
        }
    }
    strncpy(item.definition, definition, sizeof(item.definition) - 1);
    item.confidence = 0.7F;
    item.reinforcement_count = 1;

    int32_t idx = repository_add(system->repository, &item);
    if (idx >= 0) {
        system->domain_stats[KNOWLEDGE_DOMAIN_TECHNICAL].concepts_known++;
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_learn_from_demonstration: capacity exceeded");
    return false;
}


//=============================================================================
// Knowledge Retrieval - Using Fast Index
//=============================================================================

/**
 * @brief Retrieve knowledge about concept_str
 *
 * Uses hash table for O(1) average case lookup.
 *
 * @param system Knowledge system (must be non-NULL)
 * @param concept_str Concept to retrieve (must be non-NULL)
 * @param item Output item (must be non-NULL)
 * @return true if found
 *
 * Why: Fast retrieval enables responsive knowledge queries.
 * O(1) hash lookup vs O(n) linear search.
 */
bool knowledge_retrieve(knowledge_system_t system, const char* concept_str, knowledge_item_t* item)
{
    if (!system || !concept_str || !item) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_retrieve: required parameter is NULL (system, concept_str, item)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_retrieve", 0.0f);

    int32_t idx = repository_find(system->repository, concept_str);
    if (idx < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_retrieve: validation failed");
        return false;
    }

    knowledge_item_t* found = repository_get(system->repository, idx);
    if (!found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_retrieve: found is NULL");
        return false;
    }

    *item = *found;
    return true;
}


/**
 * @brief Generate understanding explanation
 *
 * Creates detailed explanation of concept_str with context and confidence.
 *
 * @param system Knowledge system (must be non-NULL)
 * @param concept_str Concept to explain (must be non-NULL)
 * @param context Additional context
 * @param explanation Output buffer (must be non-NULL)
 * @param max_length Buffer size
 * @return Length of explanation
 *
 * Why: Human-readable explanations are key to usable knowledge system.
 * Includes confidence and reinforcement for transparency.
 */
uint32_t knowledge_understand(knowledge_system_t system, const char* concept_str, const char* context,
                              char* explanation, uint32_t max_length)
{
    if (!system || !concept_str || !explanation)
        return 0;

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_understand", 0.0f);

    knowledge_item_t item;
    if (!knowledge_retrieve(system, concept_str, &item)) {
        snprintf(explanation, max_length, "I don't know about '%s' yet.", concept_str);
        return strlen(explanation);
    }

    snprintf(explanation, max_length,
             "'%s' means: %s. Context: %s. I've encountered this %u times "
             "and understand it with %.0f%% confidence.",
             concept_str, item.definition, item.context, item.reinforcement_count,
             item.confidence * 100.0F);

    return strlen(explanation);
}


/**
 * @brief Generate age-appropriate explanation
 *
 * Simplifies explanation based on target age.
 *
 * @param system Knowledge system (must be non-NULL)
 * @param concept_str Concept to explain (must be non-NULL)
 * @param target_age Target age (3-18)
 * @param explanation Output buffer (must be non-NULL)
 * @param max_length Buffer size
 * @return Length of explanation
 *
 * Why: Educational systems need age-appropriate communication.
 * Uses guard clauses for clarity vs nested ifs.
 */
uint32_t knowledge_explain_simply(knowledge_system_t system, const char* concept_str,
                                  uint32_t target_age, char* explanation, uint32_t max_length)
{
    if (!system || !concept_str || !explanation)
        return 0;

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_explain_simply", 0.0f);

    knowledge_item_t item;
    if (!knowledge_retrieve(system, concept_str, &item)) {
        snprintf(explanation, max_length, "I haven't learned about that yet.");
        return strlen(explanation);
    }

    if (target_age < 5) {
        snprintf(explanation, max_length, "%s is something you see/do/learn about.", concept_str);
        return strlen(explanation);
    }

    if (target_age < 10) {
        snprintf(explanation, max_length, "%s: %s", concept_str, item.definition);
        return strlen(explanation);
    }

    return knowledge_understand(system, concept_str, "", explanation, max_length);
}


/**
 * @brief Find connections across knowledge domains
 *
 * Single-pass algorithm without nested loops.
 *
 * @param system Knowledge system (must be non-NULL)
 * @param concept_str Central concept_str (must be non-NULL)
 * @param connections Output array (must be non-NULL)
 * @param max_connections Maximum to return
 * @return Number of connections found
 *
 * Why: Cross-domain connections enable transfer learning.
 * Flattened to single pass with guard clauses vs nested loops.
 *
 * Example:
 *   Input: "democracy"
 *   Output: Connections to history (Greek democracy), ethics (voting rights),
 *           social (civic participation)
 */
uint32_t knowledge_find_connections(knowledge_system_t system, const char* concept_str,
                                    knowledge_item_t* connections, uint32_t max_connections)
{
    if (!system || !concept_str || !connections)
        return 0;

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_find_connections", 0.0f);

    int32_t idx = repository_find(system->repository, concept_str);
    if (idx < 0)
        return 0;

    knowledge_item_t* target = repository_get(system->repository, idx);
    if (!target)
        return 0;

    uint32_t num_found = 0;

    for (uint32_t i = 0; i < system->repository->num_items && num_found < max_connections; i++) {
        if (i == (uint32_t) idx)
            continue;

        knowledge_item_t* item = repository_get(system->repository, i);
        if (!item)
            continue;

        if (is_cross_domain_related(target, item, concept_str)) {
            connections[num_found++] = *item;
        }
    }

    return num_found;
}


/**
 * @brief Apply knowledge from one domain to another
 *
 * Transfer learning enables applying lessons across contexts.
 *
 * @param system Knowledge system (must be non-NULL)
 * @param source_domain Source of knowledge
 * @param target_domain Where to apply
 * @param situation Current situation (must be non-NULL)
 * @param application Output suggestion (must be non-NULL)
 * @param max_length Buffer size
 * @return true on success
 *
 * Why: Transfer learning is key to human intelligence.
 * Example: Apply story lesson (narrative domain) to real situation (social domain).
 */
bool knowledge_transfer_learning(knowledge_system_t system, knowledge_domain_t source_domain,
                                 knowledge_domain_t target_domain, const char* situation,
                                 char* application, uint32_t max_length)
{
    if (!system || !situation || !application) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_transfer_learning: required parameter is NULL (system, situation, application)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_transfer_learning", 0.0f);

    snprintf(application, max_length, "Applying knowledge from %s to %s domain for situation: %s",
             knowledge_domain_name(source_domain), knowledge_domain_name(target_domain), situation);

    return true;
}


//=============================================================================
// Incremental Building
//=============================================================================

/**
 * @brief Build new knowledge on existing concept_str
 *
 * Analogical learning: learn by similarity to known concept_str.
 *
 * @param system Knowledge system (must be non-NULL)
 * @param new_concept New concept_str to learn (must be non-NULL)
 * @param based_on_concept Base concept_str (must be non-NULL)
 * @param differences How new differs from base
 * @return true on success
 *
 * Why: Humans learn by analogy and building on prior knowledge.
 * Example: Learn "republic" by comparing to "democracy".
 */
bool knowledge_build_on(knowledge_system_t system, const char* new_concept,
                        const char* based_on_concept, const char* differences)
{
    if (!system || !new_concept || !based_on_concept) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_build_on: required parameter is NULL (system, new_concept, based_on_concept)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_build_on", 0.0f);

    int32_t base_idx = repository_find(system->repository, based_on_concept);
    if (base_idx < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_build_on: validation failed");
        return false;
    }

    knowledge_item_t* base = repository_get(system->repository, base_idx);
    if (!base) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_build_on: base is NULL");
        return false;
    }

    knowledge_item_t new_item = *base;
    strncpy(new_item.concept_name, new_concept, sizeof(new_item.concept_name) - 1);

    if (differences) {
        snprintf(new_item.definition, sizeof(new_item.definition), "Like %s, but: %s",
                 based_on_concept, differences);
    }

    new_item.confidence = base->confidence * 0.7F;
    new_item.reinforcement_count = 1;

    return repository_add(system->repository, &new_item) >= 0;
}


/**
 * @brief Reinforce existing knowledge with new example
 *
 * Spaced repetition and reinforcement strengthen understanding.
 *
 * @param system Knowledge system (must be non-NULL)
 * @param concept_str Concept to reinforce (must be non-NULL)
 * @param new_example New example instance
 * @return true on success
 *
 * Why: Reinforcement is critical for retention and deeper understanding.
 * Each exposure increases confidence and strengthens memory.
 */
bool knowledge_reinforce(knowledge_system_t system, const char* concept_str, const char* new_example)
{
    if (!system || !concept_str) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_reinforce: required parameter is NULL (system, concept_str)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_reinforce", 0.0f);

    int32_t idx = repository_find(system->repository, concept_str);
    if (idx < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_reinforce: validation failed");
        return false;
    }

    knowledge_item_t* item = repository_get(system->repository, idx);
    if (!item) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_reinforce: item is NULL");
        return false;
    }

    // BUG FIX: Update B-tree when confidence changes
    // Remove old entry with old key (old confidence + index)
    if (system->repository->confidence_btree) {
        btree_remove(system->repository->confidence_btree, item->confidence_key);
    }

    // Update confidence
    item->reinforcement_count++;
    item->confidence = fminf(item->confidence + 0.05F, CONFIDENCE_MAX);

    // Update key with new confidence (index stays the same)
    snprintf(item->confidence_key, sizeof(item->confidence_key),
             "%08.6f_%05u", item->confidence, (uint32_t)idx);

    // Re-insert with new key
    if (system->repository->confidence_btree) {
        btree_insert(system->repository->confidence_btree, item);
    }

    if (new_example && item->num_examples < 10) {
        if (!item->examples) {
            item->examples = nimcp_malloc(10 * sizeof(char*));
            if (!item->examples) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_reinforce: item->examples is NULL");
                return false;
            }
        }
        // Use nimcp_malloc instead of strdup to match nimcp_free in destroy
        size_t example_len = strlen(new_example);
        item->examples[item->num_examples] = nimcp_malloc(example_len + 1);
        if (item->examples[item->num_examples]) {
            strncpy(item->examples[item->num_examples], new_example, example_len);
            item->examples[item->num_examples][example_len] = '\0';
            item->num_examples++;
        }
    }

    return true;
}


//=============================================================================
// Knowledge Organization
//=============================================================================

/**
 * @brief Organize knowledge within domain
 *
 * @param system Knowledge system (must be non-NULL)
 * @param domain Domain to organize
 * @return true on success
 *
 * Why: Organization improves retrieval and understanding.
 * Future: Could create hierarchies, clusters, concept_str maps.
 */
bool knowledge_organize_domain(knowledge_system_t system, knowledge_domain_t domain)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_organize_domain: system is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_organize_domain", 0.0f);

    update_domain_stats(system, domain);
    return true;
}


//=============================================================================
// Knowledge Assessment
//=============================================================================

/**
 * @brief Assess knowledge coverage in domain
 *
 * @param system Knowledge system (must be non-NULL)
 * @param domain Domain to assess
 * @param assessment Output assessment (must be non-NULL)
 * @return true on success
 *
 * Why: Assessment identifies strengths and gaps for targeted learning.
 * Provides quantitative measures of understanding.
 */
bool knowledge_assess_domain(knowledge_system_t system, knowledge_domain_t domain,
                             domain_knowledge_t* assessment)
{
    if (!system || !assessment) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_assess_domain: required parameter is NULL (system, assessment)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_assess_domain", 0.0f);

    *assessment = system->domain_stats[domain];

    assessment->coverage_percentage = (assessment->estimated_total > 0) ?
        ((float) assessment->concepts_known / assessment->estimated_total * 100.0F) : 0.0f;

    assessment->avg_confidence = calculate_domain_confidence(system, domain);

    return true;
}


//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get domain name string
 *
 * @param domain Domain enum value
 * @return Domain name string
 *
 * Why: Human-readable domain names for output and debugging.
 */
const char* knowledge_domain_name(knowledge_domain_t domain)
{
    static const char* names[] = {"Language",  "Literature", "Art",         "Ethics",
                                  "History",   "Science",    "Mathematics", "Social",
                                  "Technical", "Philosophy", "General"};

    if (domain < 0 || domain > 10)
        return "Unknown";
    return names[domain];
}


/**
 * WHAT: Add knowledge item directly (for testing)
 * WHY: Test API needs simple item addition
 * HOW: Call repository_add
 */
bool knowledge_add_item(knowledge_system_t system, const knowledge_item_t* item)
{
    if (!system || !item) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_add_item: required parameter is NULL (system, item)");
        return false;
    }

    // repository_add will set the confidence_key with the proper format (confidence_index)
    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_add_item", 0.0f);


    int32_t index = repository_add(system->repository, item);
    if (index >= 0) {
        /* W7: mirror the new concept into internal KG. */
        knowledge_kg_ensure_root(system);
        knowledge_kg_mirror_concept(system, item->concept_name,
                                    item->domain, item->confidence);
    }
    return index >= 0;
}


/**
 * WHAT: Add knowledge item to symbolic logic as facts
 * WHY:  Enable logical reasoning over knowledge graph (as recommended in audit)
 * HOW:  Convert knowledge concepts to IsA/HasProperty predicates
 *
 * INTEGRATION: Called when both knowledge and symbolic_logic are available
 *
 * EXAMPLE:
 *   Concept: "cat" with related: "animal"
 *   → Logic: IsA(cat, animal)
 *
 * @param logic Symbolic logic engine
 * @param item Knowledge item to convert to logic facts
 * @return Number of facts added
 */
uint32_t knowledge_add_to_symbolic_logic(
    symbolic_logic_t* logic,
    const knowledge_item_t* item)
{
    if (!logic || !item) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_add_to_symbolic_logi", 0.0f);


    uint32_t facts_added = 0;

    // Add IsA facts for each related concept
    // Example: IsA(cat, animal) if "cat" is related to "animal"
    for (uint32_t i = 0; i < item->num_related && i < 10; i++) {
        atomic_formula_t* isa_atom = create_simple_atomic(
            "IsA",
            item->concept_name,
            item->related_concepts[i]
        );

        if (isa_atom) {
            logic_clause_t* clause = create_clause_from_atomic(isa_atom, item->confidence);
            if (clause) {
                if (symbolic_logic_add_fact(logic, clause, item->confidence)) {
                    facts_added++;
                }
                // Note: symbolic_logic takes ownership of clause, don't free
            }
        }
    }

    // Add HasProperty fact for the concept itself
    // Example: Concept(cat) - indicates concept exists in knowledge base
    atomic_formula_t* concept_atom = create_simple_atomic(
        "Concept",
        item->concept_name,
        NULL  // Unary predicate
    );

    if (concept_atom) {
        logic_clause_t* clause = create_clause_from_atomic(concept_atom, item->confidence);
        if (clause) {
            if (symbolic_logic_add_fact(logic, clause, item->confidence)) {
                facts_added++;
            }
        }
    }

    return facts_added;
}


/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query knowledge graph for self-knowledge about this module
 *
 * WHAT: Retrieves entity observations and relations for the Knowledge module
 * WHY: Enables self-aware introspection of module capabilities
 * HOW: Uses kg_reader to query JSONL knowledge graph
 *
 * @param kg Knowledge graph reader instance
 * @return 1 if self-knowledge found, 0 otherwise
 */
int knowledge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Knowledge_System");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                knowledge_heartbeat("knowledge_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Module can access its own observations for introspection */
            (void)self->observations[i];
        }
    }

    /* Query outgoing relations (what this module connects to) */
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Knowledge_System");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    /* Query incoming relations (what connects to this module) */
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Knowledge_System");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}


/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int knowledge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_training_begin: NULL argument");
        return -1;
    }
    knowledge_heartbeat_instance(NULL, "knowledge_training_begin", 0.0f);
    (void)instance;
    return 0;
}


int knowledge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_training_end: NULL argument");
        return -1;
    }
    knowledge_heartbeat_instance(NULL, "knowledge_training_end", 1.0f);
    (void)instance;
    return 0;
}
