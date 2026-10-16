// nimcp_bio_router_part_stats.c - stats functions
// Part of nimcp_bio_router.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_bio_router.c


/**
 * WHAT: Get queue count (non-blocking)
 * WHY:  Check inbox size
 * HOW:  Lock, read count, unlock
 */
static uint32_t bio_msg_queue_count(bio_msg_queue_t* queue) {
    if (!queue) return 0;

    nimcp_platform_mutex_lock(&queue->mutex);
    uint32_t count = queue->count;
    nimcp_platform_mutex_unlock(&queue->mutex);

    return count;
}


uint32_t bio_router_inbox_count(bio_module_context_t ctx) {
    if (!ctx || ctx->magic != BIO_MODULE_MAGIC) return 0;

    bio_module_entry_t* entry = ctx->entry;
    if (!entry || entry->magic != BIO_MODULE_MAGIC) return 0;

    nimcp_platform_mutex_lock(&entry->inbox.mutex);
    uint32_t count = entry->inbox.count;
    nimcp_platform_mutex_unlock(&entry->inbox.mutex);

    return count;
}
