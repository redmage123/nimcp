/**
 * @file test_biology.cpp
 * @brief Unit tests for biology reasoning module
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_biology.h"

class BiologyTest : public ::testing::Test {
protected:
    biology_t* bio = nullptr;

    void SetUp() override {
        bio = biology_create();
        ASSERT_NE(bio, nullptr);
    }

    void TearDown() override {
        biology_destroy(bio);
        bio = nullptr;
    }

    static constexpr float FLOAT_TOLERANCE = 0.01f;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(BiologyTest, CreateDefault)
{
    EXPECT_NE(bio, nullptr);
}

TEST_F(BiologyTest, CreateCustom)
{
    biology_config_t config = biology_default_config();
    config.gap_open_penalty = -15;
    config.match_score = 3;

    biology_t* custom = biology_create_custom(&config);
    ASSERT_NE(custom, nullptr);

    biology_destroy(custom);
}

TEST_F(BiologyTest, CreateWithNullConfig)
{
    biology_t* b = biology_create_custom(nullptr);
    EXPECT_NE(b, nullptr);
    biology_destroy(b);
}

TEST_F(BiologyTest, DestroyNullSafe)
{
    biology_destroy(nullptr);  // Should not crash
}

TEST_F(BiologyTest, DefaultConfig)
{
    biology_config_t config = biology_default_config();
    EXPECT_EQ(config.gap_open_penalty, -10);
    EXPECT_EQ(config.match_score, 2);
    EXPECT_EQ(config.mismatch_penalty, -1);
}

TEST_F(BiologyTest, ValidateConfig)
{
    biology_config_t config = biology_default_config();
    EXPECT_TRUE(biology_validate_config(&config));

    config.match_score = -1;
    EXPECT_FALSE(biology_validate_config(&config));
}

TEST_F(BiologyTest, ValidateNullConfig)
{
    EXPECT_FALSE(biology_validate_config(nullptr));
}

//=============================================================================
// DNA/RNA Validation Tests
//=============================================================================

TEST_F(BiologyTest, ValidateDNA)
{
    EXPECT_TRUE(biology_validate_dna(bio, "ATGCGATCGATCG"));
    EXPECT_TRUE(biology_validate_dna(bio, "atgcgatcg"));  // lowercase
    EXPECT_FALSE(biology_validate_dna(bio, "ATGCUATCG"));  // U is RNA
    EXPECT_FALSE(biology_validate_dna(bio, "ATGCXATCG"));  // X is invalid
}

TEST_F(BiologyTest, ValidateRNA)
{
    EXPECT_TRUE(biology_validate_rna(bio, "AUGCGAUCGAUCG"));
    EXPECT_TRUE(biology_validate_rna(bio, "augcgaucg"));  // lowercase
    EXPECT_FALSE(biology_validate_rna(bio, "ATGCGAUCG"));  // T is DNA
    EXPECT_FALSE(biology_validate_rna(bio, "AUGCXAUCG"));  // X is invalid
}

//=============================================================================
// Sequence Operation Tests
//=============================================================================

TEST_F(BiologyTest, ComplementDNA)
{
    char complement[64];
    int result = biology_complement_dna(bio, "ATGC", complement, sizeof(complement));

    EXPECT_EQ(result, 0);
    EXPECT_STREQ(complement, "TACG");
}

TEST_F(BiologyTest, ReverseComplement)
{
    char rc[64];
    int result = biology_reverse_complement(bio, "ATGC", rc, sizeof(rc));

    EXPECT_EQ(result, 0);
    EXPECT_STREQ(rc, "GCAT");
}

TEST_F(BiologyTest, Transcribe)
{
    char rna[64];
    int result = biology_transcribe(bio, "ATGCGATCG", rna, sizeof(rna));

    EXPECT_EQ(result, 0);
    EXPECT_STREQ(rna, "AUGCGAUCG");
}

TEST_F(BiologyTest, Translate)
{
    char protein[64];
    // AUG = M (start), GCG = A, AUC = I, then we need more codons
    int result = biology_translate(bio, "AUGGCGAUCGAA", protein, sizeof(protein));

    EXPECT_EQ(result, 0);
    // AUG=M, GCG=A, AUC=I, GAA=E
    EXPECT_STREQ(protein, "MAIE");
}

TEST_F(BiologyTest, TranslateWithStop)
{
    char protein[64];
    // AUG = M, UAA = STOP
    int result = biology_translate(bio, "AUGUAA", protein, sizeof(protein));

    EXPECT_EQ(result, 0);
    EXPECT_STREQ(protein, "M");  // Stops at UAA
}

TEST_F(BiologyTest, CodonToAmino)
{
    EXPECT_EQ(biology_codon_to_amino(bio, "AUG"), 'M');  // Start/Methionine
    EXPECT_EQ(biology_codon_to_amino(bio, "UUU"), 'F');  // Phenylalanine
    EXPECT_EQ(biology_codon_to_amino(bio, "UAA"), '*');  // Stop
    EXPECT_EQ(biology_codon_to_amino(bio, "UAG"), '*');  // Stop
    EXPECT_EQ(biology_codon_to_amino(bio, "UGA"), '*');  // Stop
}

TEST_F(BiologyTest, GCContent)
{
    float gc = biology_gc_content(bio, "ATGC");
    EXPECT_NEAR(gc, 0.5f, FLOAT_TOLERANCE);  // 2 GC out of 4

    gc = biology_gc_content(bio, "GGCC");
    EXPECT_NEAR(gc, 1.0f, FLOAT_TOLERANCE);  // All GC

    gc = biology_gc_content(bio, "AATT");
    EXPECT_NEAR(gc, 0.0f, FLOAT_TOLERANCE);  // No GC
}

TEST_F(BiologyTest, MeltingTemp)
{
    // Short oligo: Tm = 2(A+T) + 4(G+C)
    float tm = biology_melting_temp(bio, "ATGC");  // 2*2 + 4*2 = 12
    EXPECT_NEAR(tm, 12.0f, 1.0f);
}

TEST_F(BiologyTest, SequenceNullHandling)
{
    char buffer[64];
    EXPECT_EQ(biology_complement_dna(nullptr, "ATGC", buffer, sizeof(buffer)), -1);
    EXPECT_EQ(biology_complement_dna(bio, nullptr, buffer, sizeof(buffer)), -1);
    EXPECT_EQ(biology_complement_dna(bio, "ATGC", nullptr, sizeof(buffer)), -1);
}

//=============================================================================
// Alignment Tests
//=============================================================================

TEST_F(BiologyTest, AlignGlobalIdentical)
{
    alignment_result_t result;
    int ret = biology_align_global(bio, "ATGC", "ATGC", &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.matches, 4);
    EXPECT_EQ(result.mismatches, 0);
    EXPECT_EQ(result.gaps, 0);
    EXPECT_NEAR(result.identity, 1.0f, FLOAT_TOLERANCE);
}

TEST_F(BiologyTest, AlignGlobalMismatches)
{
    alignment_result_t result;
    int ret = biology_align_global(bio, "ATGC", "TTGC", &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.matches, 3);
    EXPECT_EQ(result.mismatches, 1);
    EXPECT_NEAR(result.identity, 0.75f, FLOAT_TOLERANCE);
}

TEST_F(BiologyTest, AlignGlobalDifferentLength)
{
    alignment_result_t result;
    int ret = biology_align_global(bio, "ATGC", "AT", &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.matches, 2);
    EXPECT_GE(result.gaps, 2);
}

TEST_F(BiologyTest, SequenceSimilarity)
{
    float sim = biology_sequence_similarity(bio, "ATGC", "ATGC");
    EXPECT_NEAR(sim, 1.0f, FLOAT_TOLERANCE);

    sim = biology_sequence_similarity(bio, "ATGC", "TTTT");
    EXPECT_LT(sim, 0.5f);  // Very different
}

//=============================================================================
// Mutation Tests
//=============================================================================

TEST_F(BiologyTest, IdentifySubstitution)
{
    uint32_t pos = 0;
    mutation_type_t mut = biology_identify_mutation(bio, "ATGC", "TTGC", &pos);

    EXPECT_EQ(mut, MUTATION_SUBSTITUTION);
    EXPECT_EQ(pos, 0);
}

TEST_F(BiologyTest, IdentifyInsertion)
{
    uint32_t pos = 0;
    mutation_type_t mut = biology_identify_mutation(bio, "ATG", "ATGC", &pos);

    EXPECT_EQ(mut, MUTATION_INSERTION);
}

TEST_F(BiologyTest, IdentifyDeletion)
{
    uint32_t pos = 0;
    mutation_type_t mut = biology_identify_mutation(bio, "ATGC", "ATG", &pos);

    EXPECT_EQ(mut, MUTATION_DELETION);
}

TEST_F(BiologyTest, IsSilentMutation)
{
    // UUU and UUC both code for Phenylalanine (F)
    EXPECT_TRUE(biology_is_silent_mutation(bio, "UUU", "UUC"));

    // UUU = F, UUA = L - different amino acids
    EXPECT_FALSE(biology_is_silent_mutation(bio, "UUU", "UUA"));
}

//=============================================================================
// Phylogenetics Tests
//=============================================================================

TEST_F(BiologyTest, CreatePhyloTree)
{
    const char* species[] = {"Human", "Chimp", "Gorilla"};
    float distances[] = {
        0.0f, 0.1f, 0.2f,
        0.1f, 0.0f, 0.15f,
        0.2f, 0.15f, 0.0f
    };

    phylo_tree_t* tree = biology_create_phylo_tree(bio, species, distances, 3);
    ASSERT_NE(tree, nullptr);

    EXPECT_EQ(tree->num_leaves, 3);
    EXPECT_EQ(tree->num_internal, 2);

    biology_destroy_phylo_tree(tree);
}

TEST_F(BiologyTest, FindMRCA)
{
    const char* species[] = {"Human", "Chimp"};
    float distances[] = {0.0f, 0.1f, 0.1f, 0.0f};

    phylo_tree_t* tree = biology_create_phylo_tree(bio, species, distances, 2);
    ASSERT_NE(tree, nullptr);

    phylo_node_t* mrca = biology_find_mrca(tree, "Human", "Chimp");
    EXPECT_NE(mrca, nullptr);
    EXPECT_FALSE(mrca->is_leaf);  // MRCA should be internal node

    biology_destroy_phylo_tree(tree);
}

TEST_F(BiologyTest, EvolutionaryDistance)
{
    const char* species[] = {"A", "B"};
    float distances[] = {0.0f, 0.1f, 0.1f, 0.0f};

    phylo_tree_t* tree = biology_create_phylo_tree(bio, species, distances, 2);
    ASSERT_NE(tree, nullptr);

    float dist = biology_evolutionary_distance(tree, "A", "B");
    EXPECT_GT(dist, 0.0f);

    biology_destroy_phylo_tree(tree);
}

TEST_F(BiologyTest, PhyloTreeNullHandling)
{
    const char* species[] = {"A", "B"};
    float distances[] = {0.0f, 0.1f, 0.1f, 0.0f};

    EXPECT_EQ(biology_create_phylo_tree(nullptr, species, distances, 2), nullptr);
    EXPECT_EQ(biology_create_phylo_tree(bio, nullptr, distances, 2), nullptr);
    EXPECT_EQ(biology_create_phylo_tree(bio, species, nullptr, 2), nullptr);
    EXPECT_EQ(biology_create_phylo_tree(bio, species, distances, 1), nullptr);  // Need at least 2
}

//=============================================================================
// Population Genetics Tests
//=============================================================================

TEST_F(BiologyTest, HardyWeinberg)
{
    population_params_t params = {
        .allele_freq_p = 0.6f,
        .allele_freq_q = 0.4f,
        .population_size = 1000,
        .mutation_rate = 0.0f,
        .selection_coefficient = 0.0f,
        .migration_rate = 0.0f
    };

    hw_equilibrium_t result;
    int ret = biology_hardy_weinberg(bio, &params, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_NEAR(result.freq_AA, 0.36f, FLOAT_TOLERANCE);  // p^2
    EXPECT_NEAR(result.freq_Aa, 0.48f, FLOAT_TOLERANCE);  // 2pq
    EXPECT_NEAR(result.freq_aa, 0.16f, FLOAT_TOLERANCE);  // q^2
    EXPECT_TRUE(result.is_in_equilibrium);
}

TEST_F(BiologyTest, SelectionFrequency)
{
    population_params_t params = {
        .allele_freq_p = 0.5f,
        .allele_freq_q = 0.5f,
        .population_size = 1000,
        .mutation_rate = 0.0f,
        .selection_coefficient = 0.1f,  // Positive selection
        .migration_rate = 0.0f
    };

    float new_freq = biology_selection_frequency(bio, &params, 10);

    // With positive selection, allele frequency should increase
    EXPECT_GT(new_freq, 0.5f);
    EXPECT_LE(new_freq, 1.0f);
}

TEST_F(BiologyTest, GeneticDrift)
{
    population_params_t params = {
        .allele_freq_p = 0.5f,
        .allele_freq_q = 0.5f,
        .population_size = 100,  // Small population
        .mutation_rate = 0.0f,
        .selection_coefficient = 0.0f,
        .migration_rate = 0.0f
    };

    float variance = biology_genetic_drift(bio, &params, 10);

    EXPECT_GT(variance, 0.0f);
    EXPECT_LT(variance, 0.25f);  // Max variance is pq = 0.25
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(BiologyTest, SetInflammation)
{
    EXPECT_EQ(biology_set_inflammation(bio, 0.5f), 0);
    EXPECT_EQ(biology_set_inflammation(bio, 1.5f), 0);  // Should clamp
    EXPECT_EQ(biology_set_inflammation(bio, -0.5f), 0);  // Should clamp
}

TEST_F(BiologyTest, SetSleepDeprivation)
{
    EXPECT_EQ(biology_set_sleep_deprivation(bio, 0.5f), 0);
}

TEST_F(BiologyTest, ModulationNullHandling)
{
    EXPECT_EQ(biology_set_inflammation(nullptr, 0.5f), -1);
    EXPECT_EQ(biology_set_sleep_deprivation(nullptr, 0.5f), -1);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(BiologyTest, GetStats)
{
    // Perform some operations
    char buffer[64];
    biology_transcribe(bio, "ATGC", buffer, sizeof(buffer));
    biology_translate(bio, "AUGGCG", buffer, sizeof(buffer));

    biology_stats_t stats;
    int result = biology_get_stats(bio, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.sequences_analyzed, 1);
    EXPECT_GE(stats.translations_performed, 1);
}

TEST_F(BiologyTest, ResetStats)
{
    char buffer[64];
    biology_transcribe(bio, "ATGC", buffer, sizeof(buffer));

    biology_reset_stats(bio);

    biology_stats_t stats;
    biology_get_stats(bio, &stats);

    EXPECT_EQ(stats.sequences_analyzed, 0);
    EXPECT_EQ(stats.translations_performed, 0);
}

TEST_F(BiologyTest, StatsNullHandling)
{
    biology_stats_t stats;
    EXPECT_EQ(biology_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(biology_get_stats(bio, nullptr), -1);
}

TEST_F(BiologyTest, ResetStatsNullSafe)
{
    biology_reset_stats(nullptr);  // Should not crash
}

TEST_F(BiologyTest, GetLastError)
{
    const char* err = biology_get_last_error();
    EXPECT_NE(err, nullptr);
}
