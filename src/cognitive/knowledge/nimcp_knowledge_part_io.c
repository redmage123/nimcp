// nimcp_knowledge_part_io.c - io functions
// Part of nimcp_knowledge.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_knowledge.c


//=============================================================================
// Reading & Sources
//=============================================================================

/**
 * @brief Start reading book incrementally
 *
 * @param system Knowledge system (must be non-NULL)
 * @param book_title Book title (must be non-NULL)
 * @param book_text Full text (must be non-NULL)
 * @param reading_speed Pages per session
 * @return Number of concepts learned
 *
 * Why: Humans read books incrementally, not all at once.
 * Simulates gradual learning process.
 */
uint32_t knowledge_read_book(knowledge_system_t system, const char* book_title,
                             const char* book_text, uint32_t reading_speed)
{
    if (!system || !book_title || !book_text)
        return 0;

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_read_book", 0.0f);

    if (system->num_reading >= system->reading_capacity)
        return 0;

    reading_progress_t* reading = &system->reading_list[system->num_reading++];
    strncpy(reading->book_title, book_title, sizeof(reading->book_title) - 1);
    reading->current_page = 0;
    reading->total_pages = strlen(book_text) / 500;
    reading->comprehension_score = 0.0F;

    char excerpt[2000];
    strncpy(excerpt, book_text, sizeof(excerpt) - 1);
    excerpt[sizeof(excerpt) - 1] = '\0';

    uint32_t learned = knowledge_learn_from_text(system, excerpt, KNOWLEDGE_DOMAIN_LITERATURE);

    reading->current_page = reading_speed;
    return learned;
}


/**
 * @brief Continue reading book from bookmark
 *
 * @param system Knowledge system (must be non-NULL)
 * @param book_title Book to continue (must be non-NULL)
 * @param continue_reading Whether to continue
 * @return Progress percentage [0-100]
 *
 * Why: Persistent reading progress mirrors human learning.
 * Single-pass search with early return.
 */
uint32_t knowledge_continue_reading(knowledge_system_t system, const char* book_title,
                                    bool continue_reading)
{
    if (!system || !book_title)
        return 0;

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_continue_reading", 0.0f);

    for (uint32_t i = 0; i < system->num_reading; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_reading > 256) {
            knowledge_heartbeat("knowledge_loop",
                             (float)(i + 1) / (float)system->num_reading);
        }

        if (strcmp(system->reading_list[i].book_title, book_title) == 0) {
            // Guard clause: Prevent divide by zero
            if (system->reading_list[i].total_pages == 0)
                return 0;

            return (system->reading_list[i].current_page * 100) /
                   system->reading_list[i].total_pages;
        }
    }

    return 0;
}


/**
 * @brief Print knowledge item details
 *
 * @param item Item to print (can be NULL)
 *
 * Why: Debugging and inspection of learned knowledge.
 */
void knowledge_print_item(const knowledge_item_t* item)
{
    if (!item)
        return;

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_print_item", 0.0f);

    printf("Concept: %s\n", item->concept_name);
    printf("  Domain: %s\n", knowledge_domain_name(item->domain));
    printf("  Definition: %s\n", item->definition);
    printf("  Context: %s\n", item->context);
    printf("  Confidence: %.2f\n", item->confidence);
    printf("  Reinforcements: %u\n", item->reinforcement_count);
}


/**
 * @brief Print domain assessment
 *
 * @param assessment Assessment to print (can be NULL)
 *
 * Why: Shows learning progress and coverage in domain.
 */
void knowledge_print_assessment(const domain_knowledge_t* assessment)
{
    if (!assessment)
        return;

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_print_assessment", 0.0f);

    printf("Domain: %s\n", knowledge_domain_name(assessment->domain));
    printf("  Concepts known: %u / %u (%.1f%%)\n", assessment->concepts_known,
           assessment->estimated_total, assessment->coverage_percentage);
    printf("  Average confidence: %.2f\n", assessment->avg_confidence);
    printf("  Knowledge gaps: %u\n", assessment->num_gaps);
}


/**
 * @brief Save knowledge to file
 *
 * Persistent storage enables resuming learning sessions.
 *
 * @param system Knowledge system (must be non-NULL)
 * @param filepath Save path (must be non-NULL)
 * @return true on success
 *
 * Why: Long-term memory requires persistent storage.
 * Simplified version saves main items only.
 */
bool knowledge_save(knowledge_system_t system, const char* filepath)
{
    if (!system || !filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_save: required parameter is NULL (system, filepath)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_save", 0.0f);

    FILE* file = fopen(filepath, "wb");
    if (!file) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_save: file is NULL");
        return false;
    }

    uint32_t magic = 0x4B4E4F57;
    uint32_t version = 0x00020500;
    if (fwrite(&magic, sizeof(uint32_t), 1, file) != 1 ||
        fwrite(&version, sizeof(uint32_t), 1, file) != 1 ||
        fwrite(&system->repository->num_items, sizeof(uint32_t), 1, file) != 1 ||
        fwrite(&system->num_narratives, sizeof(uint32_t), 1, file) != 1 ||
        fwrite(&system->num_artworks, sizeof(uint32_t), 1, file) != 1 ||
        fwrite(&system->num_history, sizeof(uint32_t), 1, file) != 1) {
        fclose(file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_save: write error on header");
        return false;
    }

    for (uint32_t i = 0; i < system->repository->num_items; i++) {
        // NOTE: knowledge_item_t contains pointer fields (examples, related_concepts).
        // These are serialized as raw addresses and must be nulled on load.
        if (fwrite(&system->repository->items[i], sizeof(knowledge_item_t), 1, file) != 1) {
            fclose(file);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_save: write error on item");
            return false;
        }
    }

    fclose(file);
    return true;
}


/**
 * @brief Load knowledge from file
 *
 * Resumes learning from previous session.
 *
 * @param filepath Load path (must be non-NULL)
 * @return Loaded knowledge system or NULL on error
 *
 * Why: Enables cumulative learning across sessions.
 * Rebuilds hash index after loading.
 */
knowledge_system_t knowledge_load(const char* filepath)
{
    if (!filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_load: filepath is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_heartbeat("knowledge_load", 0.0f);

    FILE* file = fopen(filepath, "rb");
    if (!file) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_load: file is NULL");
        return NULL;
    }

    uint32_t magic, version;
    if (fread(&magic, sizeof(uint32_t), 1, file) != 1 ||
        fread(&version, sizeof(uint32_t), 1, file) != 1) {
        fclose(file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_load: short read on header");
        return NULL;
    }

    if (magic != 0x4B4E4F57) {
        fclose(file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_load: validation failed");
        return NULL;
    }

    knowledge_system_t system = knowledge_system_create("loaded");
    if (!system) {
        fclose(file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_load: system is NULL");
        return NULL;
    }

    if (fread(&system->repository->num_items, sizeof(uint32_t), 1, file) != 1 ||
        fread(&system->num_narratives, sizeof(uint32_t), 1, file) != 1 ||
        fread(&system->num_artworks, sizeof(uint32_t), 1, file) != 1 ||
        fread(&system->num_history, sizeof(uint32_t), 1, file) != 1) {
        fclose(file);
        knowledge_system_destroy(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_load: short read on counts");
        return NULL;
    }

    // Validate num_items against capacity to prevent buffer overflow
    if (system->repository->num_items > system->repository->capacity) {
        fclose(file);
        knowledge_system_destroy(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_load: num_items exceeds capacity");
        return NULL;
    }

    for (uint32_t i = 0; i < system->repository->num_items; i++) {
        size_t read_count = fread(&system->repository->items[i], sizeof(knowledge_item_t), 1, file);
        if (read_count != 1) {
            // Short read — truncate at items successfully read
            system->repository->num_items = i;
            break;
        }

        // CRITICAL: Null out pointer fields that were serialized as raw addresses.
        // These point to the old process's address space and would cause UAF/crash
        // if dereferenced (e.g., during repository_destroy).
        system->repository->items[i].examples = NULL;
        system->repository->items[i].num_examples = 0;
        system->repository->items[i].related_concepts = NULL;
        system->repository->items[i].num_related = 0;

        // Populate B-tree key field with unique key: "confidence_index"
        snprintf(system->repository->items[i].confidence_key,
                 sizeof(system->repository->items[i].confidence_key),
                 "%08.6f_%05u", system->repository->items[i].confidence, i);

        knowledge_hash_table_insert(system->repository->index, system->repository->items[i].concept_name, i);

        // Rebuild B-tree index
        if (system->repository->confidence_btree) {
            btree_insert(system->repository->confidence_btree, &system->repository->items[i]);
        }
    }

    fclose(file);
    return system;
}
