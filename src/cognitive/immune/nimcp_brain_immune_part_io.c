// nimcp_brain_immune_part_io.c - io functions
// Part of nimcp_brain_immune.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_brain_immune.c


/* ============================================================================
 * String Conversion
 * ============================================================================ */

/**
 * @brief Convert phase to string
 */
const char* brain_immune_phase_to_string(brain_immune_phase_t phase) {
    switch (phase) {
        case IMMUNE_PHASE_SURVEILLANCE: return "SURVEILLANCE";
        case IMMUNE_PHASE_RECOGNITION:  return "RECOGNITION";
        case IMMUNE_PHASE_ACTIVATION:   return "ACTIVATION";
        case IMMUNE_PHASE_EFFECTOR:     return "EFFECTOR";
        case IMMUNE_PHASE_RESOLUTION:   return "RESOLUTION";
        case IMMUNE_PHASE_MEMORY:       return "MEMORY";
        default: return "UNKNOWN";
    }
}


/**
 * @brief Convert B cell state to string
 */
const char* brain_immune_b_cell_state_to_string(brain_b_cell_state_t state) {
    switch (state) {
        case B_CELL_NAIVE:     return "NAIVE";
        case B_CELL_ACTIVATED: return "ACTIVATED";
        case B_CELL_PLASMA:    return "PLASMA";
        case B_CELL_MEMORY:    return "MEMORY";
        case B_CELL_APOPTOTIC: return "APOPTOTIC";
        default: return "UNKNOWN";
    }
}


/**
 * @brief Convert T cell type to string
 */
const char* brain_immune_t_cell_type_to_string(brain_t_cell_type_t type) {
    switch (type) {
        case T_CELL_NAIVE:      return "NAIVE";
        case T_CELL_HELPER:     return "HELPER";
        case T_CELL_KILLER:     return "KILLER";
        case T_CELL_REGULATORY: return "REGULATORY";
        case T_CELL_MEMORY:     return "MEMORY";
        default: return "UNKNOWN";
    }
}


/**
 * @brief Convert cytokine type to string
 */
const char* brain_immune_cytokine_to_string(brain_cytokine_type_t type) {
    switch (type) {
        case CYTOKINE_IL1B:       return "IL-1";
        case CYTOKINE_IL6:       return "IL-6";
        case CYTOKINE_IL10:      return "IL-10";
        case CYTOKINE_TNFA: return "TNF-alpha";
        case BRAIN_CYTOKINE_IFN_GAMMA: return "IFN-gamma";
        default: return "UNKNOWN";
    }
}


/**
 * @brief Convert inflammation level to string
 */
const char* brain_immune_inflammation_to_string(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return "NONE";
        case INFLAMMATION_LOCAL:    return "LOCAL";
        case INFLAMMATION_REGIONAL: return "REGIONAL";
        case INFLAMMATION_SYSTEMIC: return "SYSTEMIC";
        case INFLAMMATION_STORM:    return "CYTOKINE_STORM";
        default: return "UNKNOWN";
    }
}
