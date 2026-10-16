/**
 * @file nimcp_biology.h
 * @brief Biology reasoning module for parietal lobe
 *
 * Implements biological reasoning capabilities:
 * - DNA/RNA sequence analysis and manipulation
 * - Protein structure basics (amino acids, codons)
 * - Gene expression modeling
 * - Phylogenetic tree analysis
 * - Population genetics basics
 *
 * BIOLOGICAL BASIS:
 * Biology reasoning integrates spatial reasoning (for molecular structure)
 * with pattern detection (for sequence analysis) and mathematical
 * intuition (for population genetics and evolutionary models).
 *
 * USAGE:
 * ```c
 * biology_t* bio = biology_create();
 *
 * // Transcribe DNA to RNA
 * const char* dna = "ATGCGATCGATCG";
 * char rna[64];
 * biology_transcribe(bio, dna, rna, sizeof(rna));
 *
 * // Translate to amino acids
 * char protein[64];
 * biology_translate(bio, rna, protein, sizeof(protein));
 *
 * biology_destroy(bio);
 * ```
 */

#ifndef NIMCP_BIOLOGY_H
#define NIMCP_BIOLOGY_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum sequence length */
#define BIOLOGY_MAX_SEQUENCE            4096

/** Maximum species in phylogenetic tree */
#define BIOLOGY_MAX_SPECIES             64

/** Codon length */
#define BIOLOGY_CODON_LENGTH            3

/** Bio-async module ID for biology */
#define BIO_MODULE_BIOLOGY              0x0386

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for biology processor */
typedef struct biology biology_t;

/**
 * @brief Nucleotide bases
 */
typedef enum {
    NUCLEOTIDE_A,   /**< Adenine */
    NUCLEOTIDE_T,   /**< Thymine (DNA only) */
    NUCLEOTIDE_U,   /**< Uracil (RNA only) */
    NUCLEOTIDE_G,   /**< Guanine */
    NUCLEOTIDE_C    /**< Cytosine */
} nucleotide_t;

/**
 * @brief Amino acids (single letter codes)
 */
typedef enum {
    AMINO_A,    /**< Alanine */
    AMINO_R,    /**< Arginine */
    AMINO_N,    /**< Asparagine */
    AMINO_D,    /**< Aspartic acid */
    AMINO_C,    /**< Cysteine */
    AMINO_E,    /**< Glutamic acid */
    AMINO_Q,    /**< Glutamine */
    AMINO_G,    /**< Glycine */
    AMINO_H,    /**< Histidine */
    AMINO_I,    /**< Isoleucine */
    AMINO_L,    /**< Leucine */
    AMINO_K,    /**< Lysine */
    AMINO_M,    /**< Methionine (Start) */
    AMINO_F,    /**< Phenylalanine */
    AMINO_P,    /**< Proline */
    AMINO_S,    /**< Serine */
    AMINO_T,    /**< Threonine */
    AMINO_W,    /**< Tryptophan */
    AMINO_Y,    /**< Tyrosine */
    AMINO_V,    /**< Valine */
    AMINO_STOP  /**< Stop codon */
} amino_acid_t;

/**
 * @brief Mutation types
 */
typedef enum {
    MUTATION_SUBSTITUTION,      /**< Point mutation */
    MUTATION_INSERTION,         /**< Insertion */
    MUTATION_DELETION,          /**< Deletion */
    MUTATION_SILENT,            /**< No amino acid change */
    MUTATION_MISSENSE,          /**< Different amino acid */
    MUTATION_NONSENSE,          /**< Premature stop codon */
    MUTATION_FRAMESHIFT         /**< Frame shift */
} mutation_type_t;

/**
 * @brief Sequence type
 */
typedef enum {
    SEQUENCE_DNA,
    SEQUENCE_RNA,
    SEQUENCE_PROTEIN
} sequence_type_t;

/**
 * @brief Phylogenetic tree node
 */
typedef struct phylo_node {
    uint32_t id;                        /**< Node ID */
    char name[64];                      /**< Species/taxon name */
    struct phylo_node* left;            /**< Left child */
    struct phylo_node* right;           /**< Right child */
    struct phylo_node* parent;          /**< Parent node */
    float branch_length;                /**< Branch length to parent */
    bool is_leaf;                       /**< Is this a leaf node */
} phylo_node_t;

/**
 * @brief Phylogenetic tree
 */
typedef struct {
    phylo_node_t* root;                 /**< Root node */
    uint32_t num_leaves;                /**< Number of leaf nodes */
    uint32_t num_internal;              /**< Number of internal nodes */
    float total_branch_length;          /**< Sum of all branch lengths */
} phylo_tree_t;

/**
 * @brief Sequence alignment result
 */
typedef struct {
    char aligned_seq1[BIOLOGY_MAX_SEQUENCE]; /**< Aligned sequence 1 */
    char aligned_seq2[BIOLOGY_MAX_SEQUENCE]; /**< Aligned sequence 2 */
    uint32_t length;                    /**< Alignment length */
    int score;                          /**< Alignment score */
    uint32_t matches;                   /**< Number of matches */
    uint32_t mismatches;                /**< Number of mismatches */
    uint32_t gaps;                      /**< Number of gaps */
    float identity;                     /**< Percent identity */
} alignment_result_t;

/**
 * @brief Population genetics parameters
 */
typedef struct {
    float allele_freq_p;                /**< Frequency of allele p */
    float allele_freq_q;                /**< Frequency of allele q (1-p) */
    uint32_t population_size;           /**< Effective population size */
    float mutation_rate;                /**< Mutation rate per generation */
    float selection_coefficient;        /**< Selection coefficient */
    float migration_rate;               /**< Migration rate */
} population_params_t;

/**
 * @brief Hardy-Weinberg equilibrium result
 */
typedef struct {
    float freq_AA;                      /**< Frequency of AA genotype */
    float freq_Aa;                      /**< Frequency of Aa genotype */
    float freq_aa;                      /**< Frequency of aa genotype */
    bool is_in_equilibrium;             /**< Whether HW equilibrium holds */
    float chi_squared;                  /**< Chi-squared test statistic */
} hw_equilibrium_t;

/**
 * @brief Biology configuration
 */
typedef struct {
    int gap_open_penalty;               /**< Gap open penalty for alignment */
    int gap_extend_penalty;             /**< Gap extend penalty */
    int match_score;                    /**< Score for matching bases */
    int mismatch_penalty;               /**< Penalty for mismatches */
    bool use_blosum62;                  /**< Use BLOSUM62 for proteins */
    bool enable_bio_async;              /**< Enable bio-async messaging */
    float inflammation_sensitivity;     /**< Immune modulation sensitivity */
    float sleep_deprivation_factor;     /**< Sleep modulation factor */
} biology_config_t;

/**
 * @brief Biology statistics
 */
typedef struct {
    uint64_t sequences_analyzed;        /**< Sequences analyzed */
    uint64_t alignments_performed;      /**< Alignments performed */
    uint64_t translations_performed;    /**< Translation operations */
    uint64_t phylo_analyses;            /**< Phylogenetic analyses */
    float avg_processing_time_us;       /**< Average processing time */
} biology_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Create biology processor with default configuration
 *
 * @return Handle or NULL on error
 */
biology_t* biology_create(void);

/**
 * @brief Create biology processor with custom configuration
 *
 * @param config Configuration (NULL for defaults)
 * @return Handle or NULL on error
 */
biology_t* biology_create_custom(const biology_config_t* config);

/**
 * @brief Destroy biology processor
 *
 * @param bio Handle (NULL safe)
 */
void biology_destroy(biology_t* bio);

/**
 * @brief Get default configuration
 *
 * @return Default configuration struct
 */
biology_config_t biology_default_config(void);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return true if valid
 */
bool biology_validate_config(const biology_config_t* config);

/* ============================================================================
 * SEQUENCE OPERATIONS
 * ============================================================================ */

/**
 * @brief Validate a DNA sequence
 *
 * @param bio Biology handle
 * @param sequence DNA sequence (A, T, G, C)
 * @return true if valid DNA sequence
 */
bool biology_validate_dna(const biology_t* bio, const char* sequence);

/**
 * @brief Validate an RNA sequence
 *
 * @param bio Biology handle
 * @param sequence RNA sequence (A, U, G, C)
 * @return true if valid RNA sequence
 */
bool biology_validate_rna(const biology_t* bio, const char* sequence);

/**
 * @brief Get complement of DNA strand
 *
 * @param bio Biology handle
 * @param dna Input DNA sequence
 * @param complement Output buffer
 * @param buffer_size Buffer size
 * @return 0 on success
 */
int biology_complement_dna(
    biology_t* bio,
    const char* dna,
    char* complement,
    size_t buffer_size
);

/**
 * @brief Get reverse complement of DNA strand
 *
 * @param bio Biology handle
 * @param dna Input DNA sequence
 * @param reverse_complement Output buffer
 * @param buffer_size Buffer size
 * @return 0 on success
 */
int biology_reverse_complement(
    biology_t* bio,
    const char* dna,
    char* reverse_complement,
    size_t buffer_size
);

/**
 * @brief Transcribe DNA to RNA
 *
 * @param bio Biology handle
 * @param dna Input DNA sequence (template strand)
 * @param rna Output RNA buffer
 * @param buffer_size Buffer size
 * @return 0 on success
 */
int biology_transcribe(
    biology_t* bio,
    const char* dna,
    char* rna,
    size_t buffer_size
);

/**
 * @brief Translate RNA to protein sequence
 *
 * @param bio Biology handle
 * @param rna Input RNA sequence
 * @param protein Output protein (single letter amino acids)
 * @param buffer_size Buffer size
 * @return 0 on success
 */
int biology_translate(
    biology_t* bio,
    const char* rna,
    char* protein,
    size_t buffer_size
);

/**
 * @brief Get codon table lookup
 *
 * @param bio Biology handle
 * @param codon Three-letter codon (RNA)
 * @return Amino acid (single letter) or '*' for stop
 */
char biology_codon_to_amino(
    const biology_t* bio,
    const char* codon
);

/**
 * @brief Calculate GC content of sequence
 *
 * @param bio Biology handle
 * @param sequence DNA or RNA sequence
 * @return GC content as fraction [0,1]
 */
float biology_gc_content(
    const biology_t* bio,
    const char* sequence
);

/**
 * @brief Calculate melting temperature (Tm) of DNA
 *
 * @param bio Biology handle
 * @param dna DNA sequence
 * @return Approximate Tm in Celsius
 */
float biology_melting_temp(
    biology_t* bio,
    const char* dna
);

/* ============================================================================
 * SEQUENCE ALIGNMENT
 * ============================================================================ */

/**
 * @brief Global sequence alignment (Needleman-Wunsch)
 *
 * @param bio Biology handle
 * @param seq1 First sequence
 * @param seq2 Second sequence
 * @param result Output alignment result
 * @return 0 on success
 */
int biology_align_global(
    biology_t* bio,
    const char* seq1,
    const char* seq2,
    alignment_result_t* result
);

/**
 * @brief Local sequence alignment (Smith-Waterman)
 *
 * @param bio Biology handle
 * @param seq1 First sequence
 * @param seq2 Second sequence
 * @param result Output alignment result
 * @return 0 on success
 */
int biology_align_local(
    biology_t* bio,
    const char* seq1,
    const char* seq2,
    alignment_result_t* result
);

/**
 * @brief Calculate sequence similarity
 *
 * @param bio Biology handle
 * @param seq1 First sequence
 * @param seq2 Second sequence
 * @return Similarity score [0,1]
 */
float biology_sequence_similarity(
    biology_t* bio,
    const char* seq1,
    const char* seq2
);

/* ============================================================================
 * MUTATION ANALYSIS
 * ============================================================================ */

/**
 * @brief Identify mutation between sequences
 *
 * @param bio Biology handle
 * @param original Original sequence
 * @param mutated Mutated sequence
 * @param position Output position of mutation
 * @return Mutation type
 */
mutation_type_t biology_identify_mutation(
    biology_t* bio,
    const char* original,
    const char* mutated,
    uint32_t* position
);

/**
 * @brief Check if mutation is silent (no amino acid change)
 *
 * @param bio Biology handle
 * @param original_codon Original codon
 * @param mutated_codon Mutated codon
 * @return true if silent mutation
 */
bool biology_is_silent_mutation(
    const biology_t* bio,
    const char* original_codon,
    const char* mutated_codon
);

/* ============================================================================
 * PHYLOGENETICS
 * ============================================================================ */

/**
 * @brief Create phylogenetic tree from distance matrix
 *
 * @param bio Biology handle
 * @param species_names Array of species names
 * @param distances Distance matrix (row-major, symmetric)
 * @param num_species Number of species
 * @return Tree or NULL on error
 */
phylo_tree_t* biology_create_phylo_tree(
    biology_t* bio,
    const char** species_names,
    const float* distances,
    uint32_t num_species
);

/**
 * @brief Destroy phylogenetic tree
 *
 * @param tree Tree to destroy
 */
void biology_destroy_phylo_tree(phylo_tree_t* tree);

/**
 * @brief Find most recent common ancestor
 *
 * @param tree Phylogenetic tree
 * @param species1 First species name
 * @param species2 Second species name
 * @return MRCA node or NULL if not found
 */
phylo_node_t* biology_find_mrca(
    const phylo_tree_t* tree,
    const char* species1,
    const char* species2
);

/**
 * @brief Calculate evolutionary distance between species
 *
 * @param tree Phylogenetic tree
 * @param species1 First species name
 * @param species2 Second species name
 * @return Branch length distance or -1 on error
 */
float biology_evolutionary_distance(
    const phylo_tree_t* tree,
    const char* species1,
    const char* species2
);

/* ============================================================================
 * POPULATION GENETICS
 * ============================================================================ */

/**
 * @brief Calculate Hardy-Weinberg equilibrium
 *
 * @param bio Biology handle
 * @param params Population parameters
 * @param result Output equilibrium result
 * @return 0 on success
 */
int biology_hardy_weinberg(
    biology_t* bio,
    const population_params_t* params,
    hw_equilibrium_t* result
);

/**
 * @brief Calculate allele frequency change under selection
 *
 * @param bio Biology handle
 * @param params Population parameters
 * @param generations Number of generations
 * @return New allele frequency after generations
 */
float biology_selection_frequency(
    biology_t* bio,
    const population_params_t* params,
    uint32_t generations
);

/**
 * @brief Calculate genetic drift (variance in allele frequency)
 *
 * @param bio Biology handle
 * @param params Population parameters
 * @param generations Number of generations
 * @return Expected variance in allele frequency
 */
float biology_genetic_drift(
    biology_t* bio,
    const population_params_t* params,
    uint32_t generations
);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

/**
 * @brief Set inflammation level
 *
 * @param bio Biology handle
 * @param level Inflammation level [0,1]
 * @return 0 on success
 */
int biology_set_inflammation(biology_t* bio, float level);

/**
 * @brief Set sleep deprivation level
 *
 * @param bio Biology handle
 * @param level Deprivation level [0,1]
 * @return 0 on success
 */
int biology_set_sleep_deprivation(biology_t* bio, float level);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param bio Biology handle
 * @param stats Output statistics
 * @return 0 on success
 */
int biology_get_stats(biology_t* bio, biology_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param bio Biology handle
 */
void biology_reset_stats(biology_t* bio);

/**
 * @brief Get last error message
 *
 * @return Thread-local error message
 */
const char* biology_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BIOLOGY_H */
