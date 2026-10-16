// nimcp_brain_training_integration_part_helpers.c - helpers functions
// Part of nimcp_brain_training_integration.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_brain_training_integration.c


static int find_loss_slot_by_id(nimcp_brain_training_ctx_t* ctx, uint32_t loss_id)
{
    for (uint32_t i = 0; i < NIMCP_TRAINING_MAX_LOSS_CONTEXTS; i++) {
        if (ctx->loss_slots[i].active && ctx->loss_slots[i].id == loss_id) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_loss_slot_by_id: validation failed");
    return -1;
}


static int find_optimizer_slot_by_id(nimcp_brain_training_ctx_t* ctx, uint32_t optimizer_id)
{
    for (uint32_t i = 0; i < NIMCP_TRAINING_MAX_OPTIMIZER_CONTEXTS; i++) {
        if (ctx->optimizer_slots[i].active && ctx->optimizer_slots[i].id == optimizer_id) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_optimizer_slot_by_id: validation failed");
    return -1;
}


static int find_scheduler_slot_by_id(nimcp_brain_training_ctx_t* ctx, uint32_t scheduler_id)
{
    for (uint32_t i = 0; i < NIMCP_TRAINING_MAX_SCHEDULER_CONTEXTS; i++) {
        if (ctx->scheduler_slots[i].active && ctx->scheduler_slots[i].id == scheduler_id) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_scheduler_slot_by_id: validation failed");
    return -1;
}


static int find_gradmgr_slot_by_id(nimcp_brain_training_ctx_t* ctx, uint32_t gradmgr_id)
{
    for (uint32_t i = 0; i < NIMCP_TRAINING_MAX_GRADMGR_CONTEXTS; i++) {
        if (ctx->gradmgr_slots[i].active && ctx->gradmgr_slots[i].id == gradmgr_id) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_gradmgr_slot_by_id: validation failed");
    return -1;
}


/* ============================================================================
 * Portia Resource Management Integration
 * ============================================================================ */

/**
 * @brief Helper: Calculate tier multipliers based on platform tier
 *
 * WHAT: Map platform tier to batch size and LR multipliers
 * WHY:  Reduce compute when resources constrained
 * HOW:  Use predefined scaling factors per tier
 */
static void calculate_tier_multipliers(
    platform_tier_t tier,
    float* batch_multiplier,
    float* lr_multiplier)
{
    switch (tier) {
        case PLATFORM_TIER_FULL:
            *batch_multiplier = 1.0F;   /* Full batch */
            *lr_multiplier = 1.0F;      /* Normal LR */
            break;

        case PLATFORM_TIER_MEDIUM:
            *batch_multiplier = 0.75F;  /* 75% batch */
            *lr_multiplier = 0.9F;      /* 90% LR */
            break;

        case PLATFORM_TIER_CONSTRAINED:
            *batch_multiplier = 0.5F;   /* 50% batch */
            *lr_multiplier = 0.75F;     /* 75% LR */
            break;

        case PLATFORM_TIER_MINIMAL:
            *batch_multiplier = 0.25F;  /* 25% batch */
            *lr_multiplier = 0.5F;      /* 50% LR */
            break;

        default:
            *batch_multiplier = 1.0F;
            *lr_multiplier = 1.0F;
            break;
    }
}
