#!/bin/bash
# Script to execute test directory reorganization by module

set -e

REPO_ROOT="/home/bbrelin/nimcp"
cd "$REPO_ROOT"

echo "========================================="
echo "NIMCP Test Directory Reorganization"
echo "========================================="
echo ""

# Module mappings (same as analysis script)
declare -A MODULE_MAP=(
    # Plasticity modules
    ["adaptive"]="plasticity/adaptive"
    ["bcm"]="plasticity/bcm"
    ["stdp"]="plasticity/stdp"
    ["stp"]="plasticity/stp"
    ["eligibility"]="plasticity/eligibility"
    ["neuromodulator"]="plasticity/neuromodulators"
    ["attention"]="plasticity/attention"
    ["pink_noise"]="plasticity/noise"
    ["phasic_tonic"]="plasticity/neuromodulators"
    ["receptor_subtype"]="plasticity/neuromodulators"

    # Glial modules
    ["astrocyte"]="glial/astrocytes"
    ["glial"]="glial"
    ["microglia"]="glial/microglia"
    ["oligodendrocyte"]="glial/oligodendrocytes"

    # Core modules
    ["brain"]="core/brain"
    ["neuralnet"]="core/neuralnet"
    ["neuron"]="core/neuron_models"
    ["synapse"]="core/synapse_types"
    ["topology"]="core/topology"
    ["oscillation"]="core/brain_oscillations"
    ["integration"]="core/integration"

    # Brain regions
    ["visual_cortex"]="core/brain_regions"
    ["audio_cortex"]="core/brain_regions"
    ["motor_cortex"]="core/brain_regions"
    ["region"]="core/brain_regions"

    # Cognitive modules
    ["epistemic"]="cognitive/epistemic"
    ["ethics"]="cognitive/ethics"
    ["salience"]="cognitive/salience"
    ["consolidation"]="cognitive/consolidation"
    ["curiosity"]="cognitive/curiosity"
    ["knowledge"]="cognitive/knowledge"
    ["wellbeing"]="cognitive/wellbeing"
    ["working_memory"]="cognitive/working_memory"
    ["emotional_tag"]="cognitive/emotional_tagging"
    ["executive"]="cognitive/executive"
    ["sleep"]="cognitive/sleep_wake"
    ["mental_health"]="cognitive/mental_health"
    ["theory_of_mind"]="cognitive/theory_of_mind"
    ["explanation"]="cognitive/explanations"
    ["meta_learning"]="cognitive/meta_learning"
    ["predictive"]="cognitive/predictive"
    ["mirror_neuron"]="cognitive/mirror_neurons"
    ["global_workspace"]="cognitive/global_workspace"
    ["autobiographical"]="cognitive/autobiographical_memory"
    ["semantic_memory"]="cognitive/memory"
    ["systems_consolidation"]="cognitive/consolidation"
    ["engram"]="cognitive/memory"
    ["grief"]="cognitive/grief"
    ["joy"]="cognitive/joy"
    ["logic"]="cognitive/logic"
    ["bias"]="cognitive/bias"
    ["personality"]="cognitive/personality"
    ["empathetic"]="cognitive/empathetic_response"
    ["emotion_recognition"]="cognitive/emotion_recognition"

    # GPU modules
    ["gpu"]="gpu"

    # Utils modules
    ["utils"]="utils"
    ["memory"]="utils/memory"
    ["cache"]="utils/cache"
    ["config"]="utils/config"
    ["queue"]="utils/queue_manager"
    ["btree"]="utils/containers"
    ["min_heap"]="utils/containers"
    ["hyperbolic"]="utils/geometry"
    ["quantum_walk"]="utils/quantum"
    ["quantum_shannon"]="utils/quantum"
    ["shannon"]="information"
    ["thread"]="utils/thread"
    ["platform"]="utils/platform"
    ["vector_math"]="utils/numerical"

    # Security
    ["security"]="security"

    # Networking
    ["cow"]="networking/distributed"
    ["distributed"]="networking/distributed"
    ["p2p"]="networking/p2p"
    ["replication"]="networking/replication"
    ["event"]="networking/events"

    # NLP
    ["nlp"]="nlp"
    ["multimodal"]="nlp"

    # Optimization
    ["multi_objective"]="optimization"
    ["dynamic_adaptation"]="optimization"
    ["adaptive_routing"]="optimization"
    ["cross_modal"]="optimization"
    ["quantum_annealing"]="optimization/quantum_annealing"

    # Information theory
    ["mps"]="utils/tensor_networks"

    # IO
    ["vesicle"]="io"
    ["metabolic"]="io"
)

# Function to determine module for a test file
get_module_for_test() {
    local filename="$1"
    local basename=$(basename "$filename" .cpp)
    basename=$(basename "$basename" .c)

    # Remove test_ prefix
    basename="${basename#test_}"
    basename="${basename#unit_test_}"
    basename="${basename#integration_test_}"
    basename="${basename#regression_test_}"

    # Convert to lowercase for matching
    local lower=$(echo "$basename" | tr '[:upper:]' '[:lower:]')

    # Try to match patterns (more specific patterns first)
    for pattern in "${!MODULE_MAP[@]}"; do
        if [[ "$lower" == *"$pattern"* ]]; then
            echo "${MODULE_MAP[$pattern]}"
            return 0
        fi
    done

    # Default to core if no match
    echo "core"
}

# Function to reorganize tests by type
reorganize_test_type() {
    local test_type="$1"  # unit, integration, or regression
    local source_dir="test/$test_type"

    if [[ ! -d "$source_dir" ]]; then
        echo "Directory $source_dir does not exist, skipping..."
        return
    fi

    echo ""
    echo "=== Reorganizing $test_type tests ==="
    echo ""

    # Count files to process
    local total_files=$(find "$source_dir" -maxdepth 1 -type f \( -name "*.cpp" -o -name "*.c" \) | wc -l)
    local processed=0

    for file in "$source_dir"/*.cpp "$source_dir"/*.c; do
        if [[ ! -f "$file" ]]; then
            continue
        fi

        processed=$((processed + 1))
        local module=$(get_module_for_test "$file")
        local target_dir="test/$test_type/$module"

        # Create target directory if it doesn't exist
        mkdir -p "$target_dir"

        # Move the file
        local basename=$(basename "$file")
        echo "[$processed/$total_files] Moving $basename -> $test_type/$module/"
        mv "$file" "$target_dir/"
    done

    echo ""
    echo "Moved $processed $test_type test files"
}

# Backup the current test directory
echo "Creating backup of test directory..."
tar -czf test_backup_$(date +%Y%m%d_%H%M%S).tar.gz test/
echo "Backup created successfully"
echo ""

# Update todo
echo "Step 1/3: Reorganizing unit tests..."
reorganize_test_type "unit"

echo ""
echo "Step 2/3: Reorganizing integration tests..."
reorganize_test_type "integration"

echo ""
echo "Step 3/3: Reorganizing regression tests..."
reorganize_test_type "regression"

echo ""
echo "========================================="
echo "Reorganization Complete!"
echo "========================================="
echo ""
echo "Next steps:"
echo "1. Review the new structure in test/"
echo "2. Update CMakeLists.txt files to reference new paths"
echo "3. Test the build to ensure all tests are found"
echo ""
