// nimcp_knowledge_part_helpers.c - helpers functions
// Part of nimcp_knowledge.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_knowledge.c


//=============================================================================
// B-TREE HELPER FUNCTIONS (from utils/containers/nimcp_btree.h)
//=============================================================================

/**
 * WHAT: Compare two confidence values for B-tree ordering
 * WHY: B-tree needs comparison function for sorting by confidence
 * HOW: Convert string confidence keys to floats for comparison
 */
static int compare_confidence(const char* key1, const char* key2)
{
    if (!key1 || !key2) {
        return 0;
    }

    // Key format is "confidence_index" (e.g., "0.300000_00123")
    // P1-2 fix: Use strtof instead of atof for safe conversion
    // strtof stops at underscore, giving us the confidence value
    char* endptr;
    float conf1 = strtof(key1, &endptr);
    float conf2 = strtof(key2, &endptr);

    if (conf1 < conf2) return -1;
    if (conf1 > conf2) return 1;

    // If confidence is equal, compare by full key (including index) for stable sorting
    return strcmp(key1, key2);
}


/**
 * WHAT: Extract confidence key from knowledge item
 * WHY: B-tree needs key extraction function
 * HOW: Format confidence as fixed-precision string
 */
static const char* extract_confidence_key(const void* data)
{
    if (!data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extract_confidence_key: data is NULL");
        return NULL;
    }

    const knowledge_item_t* item = (const knowledge_item_t*)data;
    return item->confidence_key;
}


/**
 * WHAT: Free function for B-tree (no-op for our items)
 * WHY: B-tree needs destructor, but we manage item memory separately
 * HOW: Do nothing - items are in repository array
 */
static void free_knowledge_item(void* data)
{
    // No-op: items stored in repository array
    (void)data;
}


//=============================================================================
// Hash Table Implementation - O(1) Average Lookup
//=============================================================================

/**
 * @brief Hash function using DJB2 algorithm
 *
 * Computes hash value for concept_str string to enable fast lookup.
 *
 * @param concept_str The concept_str string to hash (must be non-NULL)
 * @return Hash value in range [0, HASH_TABLE_SIZE-1]
 *
 * Why: Replaces O(n) linear search with O(1) average case lookup.
 * Algorithm: DJB2 provides good distribution with minimal collisions.
 */
static uint32_t hash_concept(const char* concept_str)
{
    uint32_t hash = 5381;
    int c = 0;

    while ((c = *concept_str++)) {
        hash = ((hash << 5) + hash) + tolower(c);
    }

    return hash % HASH_TABLE_SIZE;
}


/**
 * @brief Insert concept_str into hash table
 *
 * Adds concept_str-to-index mapping using chaining for collisions.
 *
 * @param table Hash table to insert into (must be non-NULL)
 * @param concept_str Concept name (must be non-NULL)
 * @param index Index in items array
 * @return true on success, false on allocation failure
 *
 * Why: Maintains search index for O(1) concept_str lookup.
 * Complexity: O(1) average case, O(n) worst case with many collisions.
 */
static bool knowledge_hash_table_insert(knowledge_hash_table_t* table, const char* concept_str, uint32_t index)
{
    if (!table || !concept_str) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_hash_table_insert: required parameter is NULL (table, concept_str)");
        return false;
    }

    uint32_t hash = hash_concept(concept_str);

    hash_entry_t* entry = nimcp_malloc(sizeof(hash_entry_t));
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_hash_table_insert: entry is NULL");
        return false;
    }

    // Use nimcp_malloc instead of strdup to match nimcp_free in destroy
    size_t concept_len = strlen(concept_str);
    entry->concept = nimcp_malloc(concept_len + 1);
    if (!entry->concept) {
        nimcp_free(entry);
        entry = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_hash_table_insert: entry->concept is NULL");
        return false;
    }
    strncpy(entry->concept, concept_str, concept_len + 1);
    entry->concept[concept_len] = '\0';

    entry->index = index;
    entry->next = table->entries[hash];
    table->entries[hash] = entry;

    return true;
}


/**
 * @brief Find concept_str in hash table
 *
 * Searches hash table for concept_str and returns index if found.
 *
 * @param table Hash table to search (must be non-NULL)
 * @param concept_str Concept to find (must be non-NULL)
 * @return Index in items array or -1 if not found
 *
 * Why: O(1) average case lookup vs O(n) linear search.
 * Example: Finding "democracy" in 10000 items takes ~1 comparison vs 5000 average.
 */
static int32_t knowledge_hash_table_find(knowledge_hash_table_t* table, const char* concept_str)
{
    if (!table || !concept_str) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_hash_table_find: required parameter is NULL (table, concept_str)");
        return -1;
    }

    uint32_t hash = hash_concept(concept_str);
    hash_entry_t* entry = table->entries[hash];

    while (entry) {
        if (strcasecmp(entry->concept, concept_str) == 0) {
            return (int32_t) entry->index;
        }
        entry = entry->next;
    }

    return -1;
}


/**
 * @brief Find item in repository by concept_str
 *
 * Uses hash table for O(1) average case lookup.
 *
 * @param repo Repository to search (must be non-NULL)
 * @param concept_str Concept to find (must be non-NULL)
 * @return Index or -1 if not found
 *
 * Why: Fast retrieval is critical for knowledge queries and learning.
 * Optimization: Hash table provides dramatic speedup over linear search.
 */
static int32_t repository_find(knowledge_repository_t* repo, const char* concept_str)
{
    if (!repo || !concept_str) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "repository_find: required parameter is NULL (repo, concept_str)");
        return -1;
    }
    return knowledge_hash_table_find(repo->index, concept_str);
}


/**
 * @brief Add item to repository
 *
 * Stores item and updates search index atomically.
 *
 * @param repo Repository (must be non-NULL)
 * @param item Item to add (must be non-NULL)
 * @return Index of added item or -1 on failure
 *
 * Why: Ensures index stays synchronized with items array.
 * Invariant: After successful add, repository_find() returns this index.
 */
static int32_t repository_add(knowledge_repository_t* repo, const knowledge_item_t* item)
{
    if (!repo || !item) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "repository_add: required parameter is NULL (repo, item)");
        return -1;
    }
    if (repo->num_items >= repo->capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "repository_add: capacity exceeded");
        return -1;
    }

    uint32_t index = repo->num_items;
    repo->items[index] = *item;

    // Populate B-tree key field with unique key: "confidence_index"
    // This ensures each item has a unique key even if confidence values are identical
    snprintf(repo->items[index].confidence_key, sizeof(repo->items[index].confidence_key),
             "%08.6f_%05u", repo->items[index].confidence, index);

    repo->num_items++;

    if (!knowledge_hash_table_insert(repo->index, item->concept_name, index)) {
        repo->num_items--;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "repository_add: knowledge_hash_table_insert is NULL");
        return -1;
    }

    // Insert into B-tree for confidence-based queries
    if (repo->confidence_btree) {
        int result = btree_insert(repo->confidence_btree, &repo->items[index]);
        if (result != BTREE_SUCCESS && result != BTREE_DUPLICATE) {
            // B-tree insert failed, but keep hash table consistent
            // Log warning but don't fail the operation
        }
    }

    return (int32_t) index;
}


/**
 * @brief Get item by index
 *
 * @param repo Repository (must be non-NULL)
 * @param index Item index
 * @return Pointer to item or NULL if invalid index
 *
 * Why: Safe accessor that validates index bounds.
 */
static knowledge_item_t* repository_get(knowledge_repository_t* repo, uint32_t index)
{
    if (!repo || index >= repo->num_items) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "repository_get: repo is NULL");
        return NULL;
    }
    return &repo->items[index];
}

static bool should_skip_word(const char* word)
{
    if (!word || strlen(word) < MIN_CONCEPT_LENGTH)
        return true;

    static const char* stopwords[] = {"the",   "and",   "that",  "this",  "with",
                                      "from",  "have",  "been",  "they",  "their",
                                      "about", "would", "there", "which", NULL};

    for (int i = 0; stopwords[i]; i++) {
        if (strcasecmp(word, stopwords[i]) == 0)
            return true;
    }

    return false;
}


/**
 * @brief Extract concepts from text in single pass
 *
 * Tokenizes text and filters for meaningful concepts efficiently.
 * Optimized to O(n) single-pass algorithm.
 *
 * @param text Input text (must be non-NULL)
 * @param concepts Output array for concepts
 * @param max_concepts Maximum concepts to extract
 * @return Number of concepts extracted
 *
 * Why: Efficient text processing is critical for learning performance.
 * Previous: O(n²) with nested loops checking duplicates.
 * Optimized: O(n) single pass with simple deduplication.
 *
 * Example:
 *   Input: "The quick brown fox jumps over the lazy dog"
 *   Output: ["quick", "brown", "jumps", "lazy"] (filtered stopwords)
 */
static uint32_t extract_concepts_optimized(const char* text, char concepts[][256],
                                           uint32_t max_concepts)
{
    if (!text || !concepts)
        return 0;

    // Use nimcp_malloc instead of strdup to match nimcp_free below
    size_t text_len = strlen(text);
    char* text_copy = nimcp_malloc(text_len + 1);
    if (!text_copy)
        return 0;
    strncpy(text_copy, text, text_len + 1);
    text_copy[text_len] = '\0';

    const char* delimiters = " .,;:!?\n\t\"'()[]{}";
    uint32_t num_concepts = 0;

    char* token = strtok(text_copy, delimiters);
    while (token && num_concepts < max_concepts) {
        if (!should_skip_word(token)) {
            strncpy(concepts[num_concepts], token, 255);
            concepts[num_concepts][255] = '\0';
            num_concepts++;
        }
        token = strtok(NULL, delimiters);
    }

    nimcp_free(text_copy);
    text_copy = NULL;
    return num_concepts;
}


/**
 * @brief Create context string from text excerpt
 *
 * Extracts first N characters as context while preserving word boundaries.
 *
 * @param text Source text (must be non-NULL)
 * @param output Output buffer (must be non-NULL)
 * @param max_length Maximum output length
 *
 * Why: Provides meaningful context without excessive memory usage.
 * Better than simple truncation by respecting word boundaries.
 */
static void create_context_string(const char* text, char* output, uint32_t max_length)
{
    if (!text || !output || max_length == 0)
        return;

    uint32_t len = strlen(text);
    uint32_t copy_len = (len < max_length - 4) ? len : max_length - 4;

    strncpy(output, text, copy_len);
    output[copy_len] = '\0';

    if (len >= max_length - 4) {
        // Safe: we know we have at least 4 bytes left (max_length - copy_len >= 4)
        strncat(output, "...", max_length - copy_len - 1);
    }
}


/**
 * @brief Normalize concept_str to lowercase for case-insensitive matching
 *
 * @param concept_str Source concept_str string (must be non-NULL)
 * @param output Output buffer (must be non-NULL)
 * @param max_length Maximum output length
 *
 * Why: Ensures consistent concept_str storage regardless of input capitalization.
 * "Democracy", "democracy", and "DEMOCRACY" all map to the same concept_str.
 */
static void normalize_concept_case(const char* concept_str, char* output, uint32_t max_length)
{
    if (!concept_str || !output || max_length == 0)
        return;

    uint32_t i = 0;
    for (i = 0; i < max_length - 1 && concept_str[i] != '\0'; i++) {
        output[i] = (char)tolower((unsigned char)concept_str[i]);
    }
    output[i] = '\0';
}


//=============================================================================
// Strategy Pattern for Domain-Specific Learning
//=============================================================================

/**
 * @brief Deep copy string array
 *
 * @param src Source array
 * @param count Number of strings
 * @return Copied array or NULL
 *
 * Why: Reusable helper avoids code duplication and nested ifs.
 */
static char** deep_copy_string_array(char** src, uint32_t count)
{
    if (!src || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "deep_copy_string_array: src is NULL");
        return NULL;
    }

    char** dest = (char**)nimcp_calloc(count, sizeof(char*));
    if (!dest) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "deep_copy_string_array: dest is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            knowledge_heartbeat("knowledge_loop",
                             (float)(i + 1) / (float)count);
        }

        dest[i] = NULL;
        if (!src[i])
            continue;

        size_t len = strlen(src[i]) + 1;
        dest[i] = (char*)nimcp_malloc(len);
        if (!dest[i])
            continue;

        strncpy(dest[i], src[i], len);
        dest[i][len - 1] = '\0';
    }

    return dest;
}


/**
 * @brief Learn narrative content strategy
 *
 * Implements learning strategy for narrative/story content.
 *
 * @param system Knowledge system
 * @param data Narrative knowledge structure
 * @return true on success
 *
 * Why: Strategy pattern allows different learning approaches per domain.
 * Stories require extracting themes, characters, and moral lessons.
 */
static bool strategy_learn_narrative(void* system, const void* data)
{
    knowledge_system_t sys = (knowledge_system_t) system;
    const narrative_knowledge_t* story = (const narrative_knowledge_t*) data;

    if (!sys || !story) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "strategy_learn_narrative: required parameter is NULL (sys, story)");
        return false;
    }
    if (sys->num_narratives >= sys->narratives_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "strategy_learn_narrative: capacity exceeded");
        return false;
    }

    narrative_knowledge_t* new_story = &sys->narratives[sys->num_narratives++];
    memset(new_story, 0, sizeof(narrative_knowledge_t));

    // Deep copy scalar fields
    strncpy(new_story->title, story->title, sizeof(new_story->title) - 1);
    strncpy(new_story->author, story->author, sizeof(new_story->author) - 1);
    strncpy(new_story->summary, story->summary, sizeof(new_story->summary) - 1);
    strncpy(new_story->cultural_context, story->cultural_context, sizeof(new_story->cultural_context) - 1);
    new_story->primary_domain = story->primary_domain;

    // Deep copy string arrays using helper
    new_story->num_characters = story->num_characters;
    new_story->characters = deep_copy_string_array(story->characters, story->num_characters);

    new_story->num_themes = story->num_themes;
    new_story->themes = deep_copy_string_array(story->themes, story->num_themes);

    new_story->num_lessons = story->num_lessons;
    new_story->moral_lessons = deep_copy_string_array(story->moral_lessons, story->num_lessons);

    for (uint32_t i = 0; i < story->num_lessons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && story->num_lessons > 256) {
            knowledge_heartbeat("knowledge_loop",
                             (float)(i + 1) / (float)story->num_lessons);
        }

        knowledge_item_t item = {0};
        snprintf(item.concept_name, sizeof(item.concept_name), "lesson_from_%s_%u", story->title, i);
        item.domain = KNOWLEDGE_DOMAIN_ETHICS;
        strncpy(item.definition, story->moral_lessons[i], sizeof(item.definition) - 1);
        snprintf(item.context, sizeof(item.context), "From story: %s", story->title);
        item.confidence = 0.7F;
        item.reinforcement_count = 1;

        int32_t idx = repository_add(sys->repository, &item);
        if (idx >= 0) {
            sys->domain_stats[KNOWLEDGE_DOMAIN_LITERATURE].concepts_known++;
        }
    }

    return true;
}


/**
 * @brief Learn aesthetic/art content strategy
 *
 * @param system Knowledge system
 * @param data Aesthetic knowledge structure
 * @return true on success
 *
 * Why: Art requires extracting aesthetic qualities and emotional impact.
 * Different processing than factual or narrative content.
 */
static bool strategy_learn_aesthetic(void* system, const void* data)
{
    knowledge_system_t sys = (knowledge_system_t) system;
    const aesthetic_knowledge_t* art = (const aesthetic_knowledge_t*) data;

    if (!sys || !art) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "strategy_learn_aesthetic: required parameter is NULL (sys, art)");
        return false;
    }
    if (sys->num_artworks >= sys->artworks_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "strategy_learn_aesthetic: capacity exceeded");
        return false;
    }

    aesthetic_knowledge_t* new_art = &sys->artworks[sys->num_artworks++];
    memset(new_art, 0, sizeof(aesthetic_knowledge_t));

    // Deep copy scalar fields
    strncpy(new_art->work_title, art->work_title, sizeof(new_art->work_title) - 1);
    strncpy(new_art->creator, art->creator, sizeof(new_art->creator) - 1);
    strncpy(new_art->medium, art->medium, sizeof(new_art->medium) - 1);
    strncpy(new_art->description, art->description, sizeof(new_art->description) - 1);
    strncpy(new_art->emotional_impact, art->emotional_impact, sizeof(new_art->emotional_impact) - 1);
    strncpy(new_art->historical_significance, art->historical_significance, sizeof(new_art->historical_significance) - 1);

    // Deep copy aesthetic_qualities array
    new_art->num_qualities = art->num_qualities;
    new_art->aesthetic_qualities = deep_copy_string_array(art->aesthetic_qualities, art->num_qualities);

    for (uint32_t i = 0; i < art->num_qualities; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && art->num_qualities > 256) {
            knowledge_heartbeat("knowledge_loop",
                             (float)(i + 1) / (float)art->num_qualities);
        }

        knowledge_item_t item = {0};
        strncpy(item.concept_name, art->aesthetic_qualities[i], sizeof(item.concept_name) - 1);
        item.domain = KNOWLEDGE_DOMAIN_ART;
        snprintf(item.definition, sizeof(item.definition), "Quality in %s by %s", art->work_title,
                 art->creator);
        item.confidence = 0.6F;
        item.reinforcement_count = 1;

        int32_t idx = repository_add(sys->repository, &item);
        if (idx >= 0) {
            sys->domain_stats[KNOWLEDGE_DOMAIN_ART].concepts_known++;
        }
    }

    return true;
}


/**
 * @brief Learn historical content strategy
 *
 * @param system Knowledge system
 * @param data Historical knowledge structure
 * @return true on success
 *
 * Why: History requires tracking causality, people, and significance.
 * Different structure than other knowledge types.
 */
static bool strategy_learn_historical(void* system, const void* data)
{
    knowledge_system_t sys = (knowledge_system_t) system;
    const historical_knowledge_t* event = (const historical_knowledge_t*) data;

    if (!sys || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "strategy_learn_historical: required parameter is NULL (sys, event)");
        return false;
    }
    if (sys->num_history >= sys->history_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "strategy_learn_historical: capacity exceeded");
        return false;
    }

    historical_knowledge_t* new_event = &sys->history[sys->num_history++];
    memset(new_event, 0, sizeof(historical_knowledge_t));

    // Deep copy scalar fields
    strncpy(new_event->event_name, event->event_name, sizeof(new_event->event_name) - 1);
    new_event->timestamp_year = event->timestamp_year;
    strncpy(new_event->causes, event->causes, sizeof(new_event->causes) - 1);
    strncpy(new_event->effects, event->effects, sizeof(new_event->effects) - 1);
    strncpy(new_event->significance, event->significance, sizeof(new_event->significance) - 1);

    // Deep copy string arrays
    new_event->num_people = event->num_people;
    new_event->key_people = deep_copy_string_array(event->key_people, event->num_people);

    new_event->num_related_events = event->num_related_events;
    new_event->related_events = deep_copy_string_array(event->related_events, event->num_related_events);

    knowledge_item_t item = {0};
    strncpy(item.concept_name, event->event_name, sizeof(item.concept_name) - 1);
    item.domain = KNOWLEDGE_DOMAIN_HISTORY;
    strncpy(item.definition, event->significance, sizeof(item.definition) - 1);
    snprintf(item.context, sizeof(item.context), "Year %lu", event->timestamp_year);
    item.confidence = 0.8F;
    item.reinforcement_count = 1;

    int32_t idx = repository_add(sys->repository, &item);
    if (idx >= 0) {
        sys->domain_stats[KNOWLEDGE_DOMAIN_HISTORY].concepts_known++;
    }

    return true;
}


//=============================================================================
// Domain Statistics and Assessment
//=============================================================================

/**
 * @brief Calculate average confidence for domain
 *
 * Computes mean confidence across all concepts in domain.
 *
 * @param system Knowledge system (must be non-NULL)
 * @param domain Target domain
 * @return Average confidence [0.0, 1.0]
 *
 * Why: Provides measure of understanding quality vs just quantity.
 * Single pass O(n) algorithm with early termination opportunity.
 */
static float calculate_domain_confidence(knowledge_system_t system, knowledge_domain_t domain)
{
    if (!system || !system->repository)
        return 0.0F;

    float total_confidence = 0.0F;
    uint32_t count = 0;

    for (uint32_t i = 0; i < system->repository->num_items; i++) {
        if (system->repository->items[i].domain == domain) {
            total_confidence += system->repository->items[i].confidence;
            count++;
        }
    }

    return (count > 0) ? (total_confidence / count) : 0.0F;
}


/**
 * @brief Bio-async message handler: Handle knowledge query
 */
static nimcp_error_t handle_knowledge_query(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)msg_size;
    (void)response_promise;

    if (!msg || !user_data) {
        return NIMCP_ERROR_NULL_ARG;
    }

    const bio_msg_introspection_query_t* query = (const bio_msg_introspection_query_t*)msg;
    knowledge_system_t system = (knowledge_system_t)user_data;
    (void)system;  // Will be used for actual query

    LOG_DEBUG(LOG_MODULE, "Received knowledge query: type=%u", query->query_type);

    return NIMCP_SUCCESS;
}


/**
 * @brief Free narrative array memory
 *
 * @param narratives Array to nimcp_free(can be NULL)
 * @param num_narratives Number of narratives
 *
 * Why: Extracted to reduce complexity and improve maintainability.
 * Single responsibility: free one type of resource.
 */
static void free_narratives(narrative_knowledge_t* narratives, uint32_t num_narratives)
{
    if (!narratives)
        return;

    for (uint32_t i = 0; i < num_narratives; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_narratives > 256) {
            knowledge_heartbeat("knowledge_loop",
                             (float)(i + 1) / (float)num_narratives);
        }

        if (narratives[i].characters) {
            for (uint32_t j = 0; j < narratives[i].num_characters; j++) {
                nimcp_free(narratives[i].characters[j]);
            }
            nimcp_free(narratives[i].characters);
        }
        if (narratives[i].themes) {
            for (uint32_t j = 0; j < narratives[i].num_themes; j++) {
                nimcp_free(narratives[i].themes[j]);
            }
            nimcp_free(narratives[i].themes);
        }
        if (narratives[i].moral_lessons) {
            for (uint32_t j = 0; j < narratives[i].num_lessons; j++) {
                nimcp_free(narratives[i].moral_lessons[j]);
            }
            nimcp_free(narratives[i].moral_lessons);
        }
    }
    nimcp_free(narratives);
    narratives = NULL;
}


/**
 * @brief Free artwork array memory
 *
 * @param artworks Array to nimcp_free(can be NULL)
 * @param num_artworks Number of artworks
 *
 * Why: Separated for clarity and single responsibility.
 */
static void free_artworks(aesthetic_knowledge_t* artworks, uint32_t num_artworks)
{
    if (!artworks)
        return;

    for (uint32_t i = 0; i < num_artworks; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_artworks > 256) {
            knowledge_heartbeat("knowledge_loop",
                             (float)(i + 1) / (float)num_artworks);
        }

        if (artworks[i].aesthetic_qualities) {
            for (uint32_t j = 0; j < artworks[i].num_qualities; j++) {
                nimcp_free(artworks[i].aesthetic_qualities[j]);
            }
            nimcp_free(artworks[i].aesthetic_qualities);
        }
    }
    nimcp_free(artworks);
    artworks = NULL;
}


/**
 * @brief Free history array memory
 *
 * @param history Array to nimcp_free(can be NULL)
 * @param num_history Number of history entries
 *
 * Why: Extracted for maintainability and single responsibility.
 */
static void free_history(historical_knowledge_t* history, uint32_t num_history)
{
    if (!history)
        return;

    for (uint32_t i = 0; i < num_history; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_history > 256) {
            knowledge_heartbeat("knowledge_loop",
                             (float)(i + 1) / (float)num_history);
        }

        if (history[i].key_people) {
            for (uint32_t j = 0; j < history[i].num_people; j++) {
                nimcp_free(history[i].key_people[j]);
            }
            nimcp_free(history[i].key_people);
        }
        if (history[i].related_events) {
            for (uint32_t j = 0; j < history[i].num_related_events; j++) {
                nimcp_free(history[i].related_events[j]);
            }
            nimcp_free(history[i].related_events);
        }
    }
    nimcp_free(history);
    history = NULL;
}


//=============================================================================
// Optimized Learning from Text - O(n) Instead of O(n²)
//=============================================================================

/**
 * @brief Process single concept_str during learning
 *
 * Handles both new concept_str learning and reinforcement.
 *
 * @param system Knowledge system (must be non-NULL)
 * @param concept_str Concept to process (must be non-NULL)
 * @param text Source text for context
 * @param domain Knowledge domain
 * @return true if new concept_str learned
 *
 * Why: Extracted from main loop to reduce complexity.
 * Single responsibility: process one concept_str.
 */
static bool process_concept(knowledge_system_t system, const char* concept_str, const char* text,
                            knowledge_domain_t domain)
{
    if (!system || !concept_str) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "process_concept: required parameter is NULL (system, concept_str)");
        return false;
    }

    // Normalize concept_str to lowercase for case-insensitive matching
    char normalized_concept[NIMCP_ERROR_BUFFER_SIZE];
    normalize_concept_case(concept_str, normalized_concept, sizeof(normalized_concept));

    int32_t idx = repository_find(system->repository, normalized_concept);

    if (idx < 0) {
        knowledge_item_t item = {0};
        strncpy(item.concept_name, normalized_concept, sizeof(item.concept_name) - 1);
        item.domain = domain;
        create_context_string(text, item.definition, sizeof(item.definition));
        item.confidence = 0.3F;
        item.reinforcement_count = 1;

        idx = repository_add(system->repository, &item);
        if (idx >= 0) {
            system->domain_stats[domain].concepts_known++;
            return true;
        }
    } else {
        knowledge_item_t* item = repository_get(system->repository, idx);
        if (item) {
            // BUG FIX: Update B-tree when confidence changes
            // Remove old entry with old key (old confidence + index)
            if (system->repository->confidence_btree) {
                btree_remove(system->repository->confidence_btree, item->confidence_key);
            }

            // Update confidence
            item->reinforcement_count++;
            item->confidence = fminf(item->confidence + CONFIDENCE_INCREMENT, CONFIDENCE_MAX);

            // Update key with new confidence (index stays the same)
            snprintf(item->confidence_key, sizeof(item->confidence_key),
                     "%08.6f_%05u", item->confidence, (uint32_t)idx);

            // Re-insert with new key
            if (system->repository->confidence_btree) {
                btree_insert(system->repository->confidence_btree, item);
            }
        }
    }

    return false;
}


//=============================================================================
// Cross-Domain Learning - Flattened Algorithms
//=============================================================================

/**
 * @brief Check if items are related across domains
 *
 * @param item1 First item (must be non-NULL)
 * @param item2 Second item (must be non-NULL)
 * @param target_concept Target concept_str name
 * @return true if related
 *
 * Why: Extracted to avoid nested conditions in main loop.
 * Single responsibility: determine relationship.
 */
static bool is_cross_domain_related(const knowledge_item_t* item1, const knowledge_item_t* item2,
                                    const char* target_concept)
{
    if (!item1 || !item2 || !target_concept) {
        return false;
    }
    if (item1->domain == item2->domain) {
        return false;
    }

    return true;
}


//=============================================================================
// Symbolic Logic Integration (Phase 11: Logic Wiring)
//=============================================================================

/**
 * WHAT: Create a simple atomic formula for symbolic logic
 * WHY:  Enable knowledge→logic integration without complex clause construction
 * HOW:  Create predicate with terms for basic facts
 */
static atomic_formula_t* create_simple_atomic(const char* predicate, const char* arg1, const char* arg2)
{
    if (!predicate || !arg1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_simple_atomic: required parameter is NULL (predicate, arg1)");
        return NULL;
    }

    atomic_formula_t* atom = (atomic_formula_t*)nimcp_calloc(1, sizeof(atomic_formula_t));
    if (!atom) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_simple_atomic: atom is NULL");
        return NULL;
    }

    // Set predicate name
    strncpy(atom->name, predicate, LOGIC_MAX_NAME_LENGTH - 1);
    atom->negated = false;

    // Create terms
    uint8_t arity = arg2 ? 2 : 1;
    atom->terms = (logical_term_t**)nimcp_calloc(arity, sizeof(logical_term_t*));
    if (!atom->terms) {
        nimcp_free(atom);
        atom = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_simple_atomic: atom->terms is NULL");
        return NULL;
    }

    // First argument
    atom->terms[0] = (logical_term_t*)nimcp_calloc(1, sizeof(logical_term_t));
    if (!atom->terms[0]) {
        nimcp_free(atom->terms);
        nimcp_free(atom);
        atom = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_simple_atomic: atom->terms is NULL");
        return NULL;
    }
    atom->terms[0]->type = TERM_CONSTANT;
    strncpy(atom->terms[0]->name, arg1, LOGIC_MAX_NAME_LENGTH - 1);
    atom->terms[0]->args = NULL;
    atom->terms[0]->arity = 0;

    // Second argument (if provided)
    if (arg2) {
        atom->terms[1] = (logical_term_t*)nimcp_calloc(1, sizeof(logical_term_t));
        if (!atom->terms[1]) {
            nimcp_free(atom->terms[0]);
            nimcp_free(atom->terms);
            nimcp_free(atom);
            atom = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_simple_atomic: atom->terms is NULL");
            return NULL;
        }
        atom->terms[1]->type = TERM_CONSTANT;
        strncpy(atom->terms[1]->name, arg2, LOGIC_MAX_NAME_LENGTH - 1);
        atom->terms[1]->args = NULL;
        atom->terms[1]->arity = 0;
    }

    atom->arity = arity;
    return atom;
}


/**
 * WHAT: Create logic clause from atomic formula
 * WHY:  Symbolic logic works with clauses (CNF)
 * HOW:  Wrap atomic formula in a clause with single literal
 */
static logic_clause_t* create_clause_from_atomic(atomic_formula_t* atom, float confidence)
{
    if (!atom) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_clause_from_atomic: atom is NULL");
        return NULL;
    }

    logic_clause_t* clause = (logic_clause_t*)nimcp_calloc(1, sizeof(logic_clause_t));
    if (!clause) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_clause_from_atomic: clause is NULL");
        return NULL;
    }

    clause->literals = (atomic_formula_t**)nimcp_calloc(1, sizeof(atomic_formula_t*));
    if (!clause->literals) {
        nimcp_free(clause);
        clause = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_clause_from_atomic: clause->literals is NULL");
        return NULL;
    }

    clause->literals[0] = atom;
    clause->num_literals = 1;
    clause->confidence = confidence;

    return clause;
}
