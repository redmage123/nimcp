/**
 * @file nimcp_biology.c
 * @brief Biology reasoning implementation for parietal lobe
 *
 * Implements biological reasoning including DNA/RNA processing,
 * protein translation, sequence alignment, and population genetics.
 */

#include "cognitive/parietal/nimcp_biology.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(biology, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#include "constants/nimcp_constants.h"
#define EPSILON NIMCP_EPSILON_NUMERICAL

/* ============================================================================
 * CODON TABLE
 * ============================================================================ */

/**
 * @brief Standard genetic code (RNA codons to amino acids)
 * 64 codons mapping to 20 amino acids + stop
 */
static const char CODON_TABLE[NIMCP_ID_BUFFER_SIZE][4] = {
    /* UUU */ "F", /* UUC */ "F", /* UUA */ "L", /* UUG */ "L",
    /* UCU */ "S", /* UCC */ "S", /* UCA */ "S", /* UCG */ "S",
    /* UAU */ "Y", /* UAC */ "Y", /* UAA */ "*", /* UAG */ "*",
    /* UGU */ "C", /* UGC */ "C", /* UGA */ "*", /* UGG */ "W",
    /* CUU */ "L", /* CUC */ "L", /* CUA */ "L", /* CUG */ "L",
    /* CCU */ "P", /* CCC */ "P", /* CCA */ "P", /* CCG */ "P",
    /* CAU */ "H", /* CAC */ "H", /* CAA */ "Q", /* CAG */ "Q",
    /* CGU */ "R", /* CGC */ "R", /* CGA */ "R", /* CGG */ "R",
    /* AUU */ "I", /* AUC */ "I", /* AUA */ "I", /* AUG */ "M",
    /* ACU */ "T", /* ACC */ "T", /* ACA */ "T", /* ACG */ "T",
    /* AAU */ "N", /* AAC */ "N", /* AAA */ "K", /* AAG */ "K",
    /* AGU */ "S", /* AGC */ "S", /* AGA */ "R", /* AGG */ "R",
    /* GUU */ "V", /* GUC */ "V", /* GUA */ "V", /* GUG */ "V",
    /* GCU */ "A", /* GCC */ "A", /* GCA */ "A", /* GCG */ "A",
    /* GAU */ "D", /* GAC */ "D", /* GAA */ "E", /* GAG */ "E",
    /* GGU */ "G", /* GGC */ "G", /* GGA */ "G", /* GGG */ "G"
};

/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

struct biology {
    biology_config_t config;

    /* Modulation state */
    float inflammation_level;
    float sleep_deprivation_level;

    /* Statistics */
    uint64_t sequences_analyzed;
    uint64_t alignments_performed;
    uint64_t translations_performed;
    uint64_t phylo_analyses;
    double total_processing_time_us;

    /* Thread safety */
    nimcp_mutex_t* lock;
};

/* Thread-local error message */
static _Thread_local char g_biology_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================ */

static void set_biology_error(const char* msg) {
    strncpy(g_biology_error, msg, sizeof(g_biology_error) - 1);
    g_biology_error[sizeof(g_biology_error) - 1] = '\0';
}

/**
 * @brief Convert nucleotide to index for codon table lookup
 */
static int nucleotide_to_index(char c) {
    switch (toupper(c)) {
        case 'U': return 0;
        case 'C': return 1;
        case 'A': return 2;
        case 'G': return 3;
        default: return -1;
    }
}

/**
 * @brief Get codon table index from 3-letter codon
 */
static int codon_to_index(const char* codon) {
    int i1 = nucleotide_to_index(codon[0]);
    int i2 = nucleotide_to_index(codon[1]);
    int i3 = nucleotide_to_index(codon[2]);

    if (i1 < 0 || i2 < 0 || i3 < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "codon_to_index: validation failed");
        return -1;
    }

    return i1 * 16 + i2 * 4 + i3;
}

/**
 * @brief Get DNA complement of a base
 */
static char dna_complement(char c) {
    switch (toupper(c)) {
        case 'A': return 'T';
        case 'T': return 'A';
        case 'G': return 'C';
        case 'C': return 'G';
        default: return 'N';
    }
}

/**
 * @brief Max of two integers
 */
static int max_int(int a, int b) {
    return a > b ? a : b;
}

/**
 * @brief Max of three integers
 */
static int max3_int(int a, int b, int c) {
    return max_int(max_int(a, b), c);
}

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

biology_config_t biology_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_default_config", 0.0f);


    biology_config_t config = {
        .gap_open_penalty = -10,
        .gap_extend_penalty = -1,
        .match_score = 2,
        .mismatch_penalty = -1,
        .use_blosum62 = false,
        .enable_bio_async = false,
        .inflammation_sensitivity = 0.2f,
        .sleep_deprivation_factor = 0.15f
    };
    return config;
}

bool biology_validate_config(const biology_config_t* config) {
    if (!config) {
        set_biology_error("Null config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biology_validate_config: config is NULL");
        return false;
    }
    /* Basic sanity checks */
    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_validate_config", 0.0f);


    if (config->match_score < 0) {
        set_biology_error("Match score should be non-negative");
        return false;
    }
    return true;
}

biology_t* biology_create(void) {
    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_create", 0.0f);


    return biology_create_custom(NULL);
}

biology_t* biology_create_custom(const biology_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_create_custom", 0.0f);


    biology_config_t cfg = config ? *config : biology_default_config();

    if (!biology_validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biology_create_custom: biology_validate_config is NULL");
        return NULL;
    }

    biology_t* bio = nimcp_calloc(1, sizeof(biology_t));
    if (!bio) {
        set_biology_error("Failed to allocate biology struct");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "biology_create_custom: bio is NULL");
        return NULL;
    }

    bio->config = cfg;
    bio->inflammation_level = 0.0f;
    bio->sleep_deprivation_level = 0.0f;

    mutex_attr_t attr = {.type = MUTEX_TYPE_NORMAL};
    bio->lock = nimcp_mutex_create(&attr);
    if (!bio->lock) {
        set_biology_error("Failed to create mutex");
        nimcp_free(bio);
        bio = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "biology_create_custom: bio->lock is NULL");
        return NULL;
    }

    return bio;
}

void biology_destroy(biology_t* bio) {
    if (!bio) return;

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_destroy", 0.0f);


    if (bio->lock) {
        nimcp_mutex_destroy(bio->lock);
    }
    nimcp_free(bio);
    bio = NULL;
}

/* ============================================================================
 * SEQUENCE OPERATIONS
 * ============================================================================ */

bool biology_validate_dna(const biology_t* bio, const char* sequence) {
    if (!bio || !sequence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biology_validate_dna: required parameter is NULL (bio, sequence)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_validate_dna", 0.0f);


    for (const char* p = sequence; *p; p++) {
        char c = toupper(*p);
        if (c != 'A' && c != 'T' && c != 'G' && c != 'C') {
            return false;
        }
    }
    return true;
}

bool biology_validate_rna(const biology_t* bio, const char* sequence) {
    if (!bio || !sequence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biology_validate_rna: required parameter is NULL (bio, sequence)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_validate_rna", 0.0f);


    for (const char* p = sequence; *p; p++) {
        char c = toupper(*p);
        if (c != 'A' && c != 'U' && c != 'G' && c != 'C') {
            return false;
        }
    }
    return true;
}

int biology_complement_dna(
    biology_t* bio,
    const char* dna,
    char* complement,
    size_t buffer_size
) {
    if (!bio || !dna || !complement || buffer_size == 0) {
        set_biology_error("Null parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biology_complement_dna: required parameter is NULL (bio, dna, complement)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_complement_dna", 0.0f);


    nimcp_mutex_lock(bio->lock);

    size_t len = strlen(dna);
    if (len >= buffer_size) {
        set_biology_error("Buffer too small");
        nimcp_mutex_unlock(bio->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "biology_complement_dna: capacity exceeded");
        return -1;
    }

    for (size_t i = 0; i < len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && len > 256) {
            biology_heartbeat("biology_loop",
                             (float)(i + 1) / (float)len);
        }

        complement[i] = dna_complement(dna[i]);
    }
    complement[len] = '\0';

    bio->sequences_analyzed++;
    nimcp_mutex_unlock(bio->lock);
    return 0;
}

int biology_reverse_complement(
    biology_t* bio,
    const char* dna,
    char* reverse_complement,
    size_t buffer_size
) {
    if (!bio || !dna || !reverse_complement || buffer_size == 0) {
        set_biology_error("Null parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biology_reverse_complement: required parameter is NULL (bio, dna, reverse_complement)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_reverse_complement", 0.0f);


    nimcp_mutex_lock(bio->lock);

    size_t len = strlen(dna);
    if (len >= buffer_size) {
        set_biology_error("Buffer too small");
        nimcp_mutex_unlock(bio->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "biology_reverse_complement: capacity exceeded");
        return -1;
    }

    for (size_t i = 0; i < len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && len > 256) {
            biology_heartbeat("biology_loop",
                             (float)(i + 1) / (float)len);
        }

        reverse_complement[i] = dna_complement(dna[len - 1 - i]);
    }
    reverse_complement[len] = '\0';

    bio->sequences_analyzed++;
    nimcp_mutex_unlock(bio->lock);
    return 0;
}

int biology_transcribe(
    biology_t* bio,
    const char* dna,
    char* rna,
    size_t buffer_size
) {
    if (!bio || !dna || !rna || buffer_size == 0) {
        set_biology_error("Null parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biology_transcribe: required parameter is NULL (bio, dna, rna)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_transcribe", 0.0f);


    nimcp_mutex_lock(bio->lock);

    size_t len = strlen(dna);
    if (len >= buffer_size) {
        set_biology_error("Buffer too small");
        nimcp_mutex_unlock(bio->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "biology_transcribe: capacity exceeded");
        return -1;
    }

    /* Transcription: T -> U, others stay same */
    for (size_t i = 0; i < len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && len > 256) {
            biology_heartbeat("biology_loop",
                             (float)(i + 1) / (float)len);
        }

        char c = toupper(dna[i]);
        if (c == 'T') {
            rna[i] = 'U';
        } else {
            rna[i] = c;
        }
    }
    rna[len] = '\0';

    bio->sequences_analyzed++;
    nimcp_mutex_unlock(bio->lock);
    return 0;
}

int biology_translate(
    biology_t* bio,
    const char* rna,
    char* protein,
    size_t buffer_size
) {
    if (!bio || !rna || !protein || buffer_size == 0) {
        set_biology_error("Null parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biology_translate: required parameter is NULL (bio, rna, protein)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_translate", 0.0f);


    nimcp_mutex_lock(bio->lock);

    size_t rna_len = strlen(rna);
    size_t protein_idx = 0;

    for (size_t i = 0; i + 2 < rna_len && protein_idx < buffer_size - 1; i += 3) {
        char codon[4] = {rna[i], rna[i+1], rna[i+2], '\0'};
        int idx = codon_to_index(codon);

        if (idx < 0) {
            protein[protein_idx++] = 'X';  /* Unknown */
        } else {
            char aa = CODON_TABLE[idx][0];
            if (aa == '*') {
                break;  /* Stop codon */
            }
            protein[protein_idx++] = aa;
        }
    }
    protein[protein_idx] = '\0';

    bio->translations_performed++;
    nimcp_mutex_unlock(bio->lock);
    return 0;
}

char biology_codon_to_amino(const biology_t* bio, const char* codon) {
    if (!bio || !codon || strlen(codon) != 3) return 'X';

    int idx = codon_to_index(codon);
    if (idx < 0) return 'X';

    return CODON_TABLE[idx][0];
}

float biology_gc_content(const biology_t* bio, const char* sequence) {
    if (!bio || !sequence) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_gc_content", 0.0f);


    size_t gc_count = 0;
    size_t total = 0;

    for (const char* p = sequence; *p; p++) {
        char c = toupper(*p);
        if (c == 'G' || c == 'C') gc_count++;
        if (c == 'A' || c == 'T' || c == 'G' || c == 'C' || c == 'U') total++;
    }

    if (total == 0) return 0.0f;
    return (float)gc_count / (float)total;
}

float biology_melting_temp(biology_t* bio, const char* dna) {
    if (!bio || !dna) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_melting_temp", 0.0f);


    size_t len = strlen(dna);
    if (len == 0) return 0.0f;

    size_t at_count = 0;
    size_t gc_count = 0;

    for (const char* p = dna; *p; p++) {
        char c = toupper(*p);
        if (c == 'A' || c == 'T') at_count++;
        if (c == 'G' || c == 'C') gc_count++;
    }

    /* Wallace rule for short oligos */
    if (len < 14) {
        return (float)(2 * at_count + 4 * gc_count);
    }

    /* More accurate formula for longer sequences */
    float gc_content = (float)gc_count / (float)len;
    return 81.5f + 16.6f * log10f(0.05f) + 41.0f * gc_content - 675.0f / (float)len;
}

/* ============================================================================
 * SEQUENCE ALIGNMENT
 * ============================================================================ */

int biology_align_global(
    biology_t* bio,
    const char* seq1,
    const char* seq2,
    alignment_result_t* result
) {
    if (!bio || !seq1 || !seq2 || !result) {
        set_biology_error("Null parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biology_align_global: required parameter is NULL (bio, seq1, seq2, result)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_align_global", 0.0f);


    nimcp_mutex_lock(bio->lock);

    size_t len1 = strlen(seq1);
    size_t len2 = strlen(seq2);

    if (len1 >= BIOLOGY_MAX_SEQUENCE || len2 >= BIOLOGY_MAX_SEQUENCE) {
        set_biology_error("Sequence too long");
        nimcp_mutex_unlock(bio->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "biology_align_global: capacity exceeded");
        return -1;
    }

    memset(result, 0, sizeof(alignment_result_t));

    /* Simple alignment - just compare directly for now */
    /* Full Needleman-Wunsch would require dynamic programming matrix */
    size_t min_len = len1 < len2 ? len1 : len2;
    size_t matches = 0;
    size_t mismatches = 0;

    for (size_t i = 0; i < min_len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && min_len > 256) {
            biology_heartbeat("biology_loop",
                             (float)(i + 1) / (float)min_len);
        }

        if (toupper(seq1[i]) == toupper(seq2[i])) {
            matches++;
            result->aligned_seq1[i] = seq1[i];
            result->aligned_seq2[i] = seq2[i];
        } else {
            mismatches++;
            result->aligned_seq1[i] = seq1[i];
            result->aligned_seq2[i] = seq2[i];
        }
    }

    /* Handle different lengths */
    size_t gaps = 0;
    if (len1 > min_len) {
        for (size_t i = min_len; i < len1; i++) {
            result->aligned_seq1[i] = seq1[i];
            result->aligned_seq2[i] = '-';
            gaps++;
        }
        result->length = (uint32_t)len1;
    } else if (len2 > min_len) {
        for (size_t i = min_len; i < len2; i++) {
            result->aligned_seq1[i] = '-';
            result->aligned_seq2[i] = seq2[i];
            gaps++;
        }
        result->length = (uint32_t)len2;
    } else {
        result->length = (uint32_t)min_len;
    }

    result->aligned_seq1[result->length] = '\0';
    result->aligned_seq2[result->length] = '\0';

    result->matches = (uint32_t)matches;
    result->mismatches = (uint32_t)mismatches;
    result->gaps = (uint32_t)gaps;
    result->identity = result->length > 0 ?
        (float)matches / (float)result->length : 0.0f;
    result->score = (int)matches * bio->config.match_score +
                    (int)mismatches * bio->config.mismatch_penalty +
                    (int)gaps * bio->config.gap_open_penalty;

    bio->alignments_performed++;
    nimcp_mutex_unlock(bio->lock);
    return 0;
}

int biology_align_local(
    biology_t* bio,
    const char* seq1,
    const char* seq2,
    alignment_result_t* result
) {
    /* For now, use same as global */
    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_align_local", 0.0f);


    return biology_align_global(bio, seq1, seq2, result);
}

float biology_sequence_similarity(
    biology_t* bio,
    const char* seq1,
    const char* seq2
) {
    if (!bio || !seq1 || !seq2) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_sequence_similarity", 0.0f);


    alignment_result_t result;
    if (biology_align_global(bio, seq1, seq2, &result) != 0) {
        return 0.0f;
    }

    return result.identity;
}

/* ============================================================================
 * MUTATION ANALYSIS
 * ============================================================================ */

mutation_type_t biology_identify_mutation(
    biology_t* bio,
    const char* original,
    const char* mutated,
    uint32_t* position
) {
    if (!bio || !original || !mutated) return MUTATION_SUBSTITUTION;

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_identify_mutation", 0.0f);


    size_t len_orig = strlen(original);
    size_t len_mut = strlen(mutated);

    if (position) *position = 0;

    if (len_orig == len_mut) {
        /* Same length - likely substitution */
        for (size_t i = 0; i < len_orig; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && len_orig > 256) {
                biology_heartbeat("biology_loop",
                                 (float)(i + 1) / (float)len_orig);
            }

            if (toupper(original[i]) != toupper(mutated[i])) {
                if (position) *position = (uint32_t)i;
                return MUTATION_SUBSTITUTION;
            }
        }
        return MUTATION_SILENT;  /* No difference */
    } else if (len_mut > len_orig) {
        if (position) *position = 0;
        if (len_mut - len_orig == 1) {
            return MUTATION_INSERTION;
        }
        return MUTATION_FRAMESHIFT;
    } else {
        if (position) *position = 0;
        if (len_orig - len_mut == 1) {
            return MUTATION_DELETION;
        }
        return MUTATION_FRAMESHIFT;
    }
}

bool biology_is_silent_mutation(
    const biology_t* bio,
    const char* original_codon,
    const char* mutated_codon
) {
    if (!bio || !original_codon || !mutated_codon) {
        return false;
    }
    if (strlen(original_codon) != 3 || strlen(mutated_codon) != 3) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_is_silent_mutation", 0.0f);


    char aa1 = biology_codon_to_amino(bio, original_codon);
    char aa2 = biology_codon_to_amino(bio, mutated_codon);

    return aa1 == aa2;
}

/* ============================================================================
 * PHYLOGENETICS
 * ============================================================================ */

phylo_tree_t* biology_create_phylo_tree(
    biology_t* bio,
    const char** species_names,
    const float* distances,
    uint32_t num_species
) {
    if (!bio || !species_names || !distances || num_species < 2) {
        set_biology_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biology_create_phylo_tree: required parameter is NULL (bio, species_names, distances)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_create_phylo_tree", 0.0f);


    if (num_species > BIOLOGY_MAX_SPECIES) {
        set_biology_error("Too many species");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "biology_create_phylo_tree: validation failed");
        return NULL;
    }

    nimcp_mutex_lock(bio->lock);

    phylo_tree_t* tree = nimcp_calloc(1, sizeof(phylo_tree_t));
    if (!tree) {
        set_biology_error("Failed to allocate tree");
        nimcp_mutex_unlock(bio->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "biology_create_phylo_tree: tree is NULL");
        return NULL;
    }

    /* Create leaf nodes */
    phylo_node_t** nodes = nimcp_calloc(num_species, sizeof(phylo_node_t*));
    if (!nodes) {
        nimcp_free(tree);
        nimcp_mutex_unlock(bio->lock);
        return NULL;
    }
    for (uint32_t i = 0; i < num_species; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_species > 256) {
            biology_heartbeat("biology_loop",
                             (float)(i + 1) / (float)num_species);
        }

        nodes[i] = nimcp_calloc(1, sizeof(phylo_node_t));
        if (!nodes[i]) {
            /* Free previously allocated nodes */
            for (uint32_t k = 0; k < i; k++) {
                if (nodes[k]) nimcp_free(nodes[k]);
            }
            nimcp_free(nodes);
            nimcp_free(tree);
            nimcp_mutex_unlock(bio->lock);
            return NULL;
        }
        nodes[i]->id = i;
        strncpy(nodes[i]->name, species_names[i], sizeof(nodes[i]->name) - 1);
        nodes[i]->is_leaf = true;
    }

    /* Simple UPGMA-like clustering */
    uint32_t remaining = num_species;
    uint32_t next_id = num_species;

    while (remaining > 1) {
        /* Find minimum distance pair */
        float min_dist = INFINITY;
        uint32_t min_i = 0, min_j = 1;

        for (uint32_t i = 0; i < num_species; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_species > 256) {
                biology_heartbeat("biology_loop",
                                 (float)(i + 1) / (float)num_species);
            }

            if (!nodes[i]) continue;
            for (uint32_t j = i + 1; j < num_species; j++) {
                if (!nodes[j]) continue;
                float d = distances[i * num_species + j];
                if (d < min_dist) {
                    min_dist = d;
                    min_i = i;
                    min_j = j;
                }
            }
        }

        /* Create internal node */
        phylo_node_t* internal = nimcp_calloc(1, sizeof(phylo_node_t));
        if (!internal) {
            /* Free all allocated nodes */
            for (uint32_t k = 0; k < num_species; k++) {
                if (nodes[k]) nimcp_free(nodes[k]);
            }
            nimcp_free(nodes);
            nimcp_free(tree);
            nimcp_mutex_unlock(bio->lock);
            return NULL;
        }
        internal->id = next_id++;
        internal->left = nodes[min_i];
        internal->right = nodes[min_j];
        internal->is_leaf = false;
        internal->branch_length = min_dist / 2.0f;

        nodes[min_i]->parent = internal;
        nodes[min_i]->branch_length = min_dist / 2.0f;
        nodes[min_j]->parent = internal;
        nodes[min_j]->branch_length = min_dist / 2.0f;

        tree->total_branch_length += min_dist;

        nodes[min_i] = internal;
        nodes[min_j] = NULL;
        remaining--;
    }

    /* Find root */
    for (uint32_t i = 0; i < num_species; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_species > 256) {
            biology_heartbeat("biology_loop",
                             (float)(i + 1) / (float)num_species);
        }

        if (nodes[i]) {
            tree->root = nodes[i];
            break;
        }
    }

    tree->num_leaves = num_species;
    tree->num_internal = num_species - 1;

    nimcp_free(nodes);
    nodes = NULL;
    bio->phylo_analyses++;
    nimcp_mutex_unlock(bio->lock);

    return tree;
}

static void destroy_phylo_node(phylo_node_t* node) {
    if (!node) return;
    destroy_phylo_node(node->left);
    destroy_phylo_node(node->right);
    nimcp_free(node);
    node = NULL;
}

void biology_destroy_phylo_tree(phylo_tree_t* tree) {
    if (!tree) return;
    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_destroy_phylo_tree", 0.0f);


    destroy_phylo_node(tree->root);
    nimcp_free(tree);
    tree = NULL;
}

static phylo_node_t* find_node_by_name(phylo_node_t* node, const char* name) {
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }
    if (node->is_leaf && strcmp(node->name, name) == 0) {
        return node;
    }
    phylo_node_t* found = find_node_by_name(node->left, name);
    if (found) return found;
    return find_node_by_name(node->right, name);
}

phylo_node_t* biology_find_mrca(
    const phylo_tree_t* tree,
    const char* species1,
    const char* species2
) {
    if (!tree || !species1 || !species2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biology_find_mrca: required parameter is NULL (tree, species1, species2)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_find_mrca", 0.0f);


    phylo_node_t* n1 = find_node_by_name(tree->root, species1);
    phylo_node_t* n2 = find_node_by_name(tree->root, species2);

    if (!n1 || !n2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biology_find_mrca: required parameter is NULL (n1, n2)");
        return NULL;
    }

    /* Find MRCA by traversing ancestors */
    /* Simple O(n^2) approach */
    phylo_node_t* p1 = n1;
    while (p1) {
        phylo_node_t* p2 = n2;
        while (p2) {
            if (p1 == p2) return p1;
            p2 = p2->parent;
        }
        p1 = p1->parent;
    }

    return tree->root;
}

float biology_evolutionary_distance(
    const phylo_tree_t* tree,
    const char* species1,
    const char* species2
) {
    if (!tree || !species1 || !species2) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_evolutionary_distanc", 0.0f);


    phylo_node_t* n1 = find_node_by_name(tree->root, species1);
    phylo_node_t* n2 = find_node_by_name(tree->root, species2);

    if (!n1 || !n2) return -1.0f;

    phylo_node_t* mrca = biology_find_mrca(tree, species1, species2);
    if (!mrca) return -1.0f;

    /* Sum branch lengths from each node to MRCA */
    float dist = 0.0f;

    phylo_node_t* p = n1;
    while (p && p != mrca) {
        dist += p->branch_length;
        p = p->parent;
    }

    p = n2;
    while (p && p != mrca) {
        dist += p->branch_length;
        p = p->parent;
    }

    return dist;
}

/* ============================================================================
 * POPULATION GENETICS
 * ============================================================================ */

int biology_hardy_weinberg(
    biology_t* bio,
    const population_params_t* params,
    hw_equilibrium_t* result
) {
    if (!bio || !params || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biology_hardy_weinberg: required parameter is NULL (bio, params, result)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_hardy_weinberg", 0.0f);


    nimcp_mutex_lock(bio->lock);

    float p = params->allele_freq_p;
    float q = params->allele_freq_q;

    /* HW equilibrium frequencies */
    result->freq_AA = p * p;
    result->freq_Aa = 2.0f * p * q;
    result->freq_aa = q * q;

    /* Check if in equilibrium (sum should equal 1) */
    float sum = result->freq_AA + result->freq_Aa + result->freq_aa;
    result->is_in_equilibrium = fabsf(sum - 1.0f) < EPSILON;
    result->chi_squared = 0.0f;  /* Would need observed values */

    nimcp_mutex_unlock(bio->lock);
    return 0;
}

float biology_selection_frequency(
    biology_t* bio,
    const population_params_t* params,
    uint32_t generations
) {
    if (!bio || !params) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_selection_frequency", 0.0f);


    float p = params->allele_freq_p;
    float s = params->selection_coefficient;

    /* Simple model: delta_p = spq(q + h*2pq) / mean_fitness */
    /* Simplified: assuming complete dominance */
    for (uint32_t g = 0; g < generations; g++) {
        /* Phase 8: Loop progress heartbeat */
        if ((g & 0xFF) == 0 && generations > 256) {
            biology_heartbeat("biology_loop",
                             (float)(g + 1) / (float)generations);
        }

        float q = 1.0f - p;
        float w_bar = 1.0f - s * q * q;
        float delta_p = (s * p * q * q) / w_bar;
        p = nimcp_clamp01(p + delta_p);
    }

    return p;
}

float biology_genetic_drift(
    biology_t* bio,
    const population_params_t* params,
    uint32_t generations
) {
    if (!bio || !params) return 0.0f;
    if (params->population_size == 0) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_genetic_drift", 0.0f);


    float p = params->allele_freq_p;
    float N = (float)params->population_size;

    /* Variance in allele frequency due to drift */
    /* Var(p) = p*q * (1 - (1-1/2N)^t) */
    float q = 1.0f - p;
    float drift_factor = 1.0f - powf(1.0f - 1.0f/(2.0f*N), (float)generations);

    return p * q * drift_factor;
}

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

int biology_set_inflammation(biology_t* bio, float level) {
    if (!bio) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biology_set_inflammation: bio is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_set_inflammation", 0.0f);


    nimcp_mutex_lock(bio->lock);
    bio->inflammation_level = nimcp_clamp01(level);
    nimcp_mutex_unlock(bio->lock);

    return 0;
}

int biology_set_sleep_deprivation(biology_t* bio, float level) {
    if (!bio) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biology_set_sleep_deprivation: bio is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_set_sleep_deprivatio", 0.0f);


    nimcp_mutex_lock(bio->lock);
    bio->sleep_deprivation_level = nimcp_clamp01(level);
    nimcp_mutex_unlock(bio->lock);

    return 0;
}

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

int biology_get_stats(biology_t* bio, biology_stats_t* stats) {
    if (!bio || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "biology_get_stats: required parameter is NULL (bio, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_get_stats", 0.0f);


    nimcp_mutex_lock(((biology_t*)bio)->lock);

    stats->sequences_analyzed = bio->sequences_analyzed;
    stats->alignments_performed = bio->alignments_performed;
    stats->translations_performed = bio->translations_performed;
    stats->phylo_analyses = bio->phylo_analyses;

    uint64_t total_ops = bio->sequences_analyzed + bio->alignments_performed +
                         bio->translations_performed + bio->phylo_analyses;
    if (total_ops > 0) {
        stats->avg_processing_time_us =
            (float)(bio->total_processing_time_us / (double)total_ops);
    } else {
        stats->avg_processing_time_us = 0.0f;
    }

    nimcp_mutex_unlock(((biology_t*)bio)->lock);
    return 0;
}

void biology_reset_stats(biology_t* bio) {
    if (!bio) return;

    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_reset_stats", 0.0f);


    nimcp_mutex_lock(bio->lock);
    bio->sequences_analyzed = 0;
    bio->alignments_performed = 0;
    bio->translations_performed = 0;
    bio->phylo_analyses = 0;
    bio->total_processing_time_us = 0.0;
    nimcp_mutex_unlock(bio->lock);
}

const char* biology_get_last_error(void) {
    return g_biology_error;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int biology_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    biology_heartbeat("biology_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Biology_Reasoning");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                biology_heartbeat("biology_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Module self-knowledge logged */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Biology_Reasoning");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Biology_Reasoning");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void biology_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_biology_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int biology_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "biology_training_begin: NULL argument");
        return -1;
    }
    biology_heartbeat_instance(g_biology_health_agent, "biology_training_begin", 0.0f);
    (void)(struct biology*)instance; /* Module state available for reset */
    return 0;
}

int biology_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "biology_training_end: NULL argument");
        return -1;
    }
    biology_heartbeat_instance(g_biology_health_agent, "biology_training_end", 1.0f);
    (void)(struct biology*)instance; /* Module state available for finalization */
    return 0;
}

int biology_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "biology_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    biology_heartbeat_instance(g_biology_health_agent, "biology_training_step", progress);
    (void)(struct biology*)instance; /* Module state available for step adaptation */
    return 0;
}
