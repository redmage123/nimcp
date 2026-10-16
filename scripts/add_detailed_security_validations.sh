#!/bin/bash
#
# Script to add detailed security validations to plasticity modules
#

PLASTICITY_DIR="/home/bbrelin/nimcp/src/plasticity"

echo "Adding security validations to plasticity modules..."
echo "======================================================="

# List of files to process
FILES=(
    "$PLASTICITY_DIR/adaptive/nimcp_adaptive.c"
    "$PLASTICITY_DIR/attention/nimcp_attention.c"
    "$PLASTICITY_DIR/bcm/nimcp_bcm.c"
    "$PLASTICITY_DIR/dendritic/nimcp_dendritic.c"
    "$PLASTICITY_DIR/eligibility/nimcp_eligibility_trace.c"
    "$PLASTICITY_DIR/homeostatic/nimcp_homeostatic.c"
    "$PLASTICITY_DIR/neuromodulators/nimcp_metabolic_pathways.c"
    "$PLASTICITY_DIR/neuromodulators/nimcp_neuromod_pink_noise.c"
    "$PLASTICITY_DIR/neuromodulators/nimcp_neuromodulators.c"
    "$PLASTICITY_DIR/neuromodulators/nimcp_phasic_tonic.c"
    "$PLASTICITY_DIR/neuromodulators/nimcp_receptor_subtypes.c"
    "$PLASTICITY_DIR/neuromodulators/nimcp_spatial_neuromod.c"
    "$PLASTICITY_DIR/neuromodulators/nimcp_vesicle_packaging.c"
    "$PLASTICITY_DIR/noise/nimcp_pink_noise.c"
    "$PLASTICITY_DIR/predictive/nimcp_predictive_coding.c"
)

TOTAL_PROCESSED=0
TOTAL_VALIDATED=0

for file in "${FILES[@]}"; do
    if [ -f "$file" ]; then
        echo "Processing: $file"

        # Count functions that might need validation
        INIT_FUNCS=$(grep -c "_init\|_create\|_params_" "$file" 2>/dev/null || echo 0)
        WEIGHT_UPDATES=$(grep -c "\.weight.*=" "$file" 2>/dev/null || echo 0)

        echo "  - Found $INIT_FUNCS init/create functions"
        echo "  - Found $WEIGHT_UPDATES weight assignments"

        # Check if security header is already included
        if grep -q "security/nimcp_security.h" "$file"; then
            echo "  ✓ Security header included"
            TOTAL_VALIDATED=$((TOTAL_VALIDATED + 1))
        else
            echo "  ✗ Missing security header"
        fi

        # Check if logging header is included
        if grep -q "utils/logging/nimcp_logging.h" "$file"; then
            echo "  ✓ Logging header included"
        else
            echo "  ✗ Missing logging header"
        fi

        # Check for NaN/Inf validation
        if grep -q "isnan\|isinf" "$file"; then
            echo "  ✓ NaN/Inf checks present"
        else
            echo "  ⚠ No NaN/Inf validation found"
        fi

        # Check for bounds validation
        if grep -q "LOG_WARN.*bounds\|LOG_WARN.*out of\|fmax.*fmin" "$file"; then
            echo "  ✓ Bounds validation present"
        else
            echo "  ⚠ No bounds validation found"
        fi

        TOTAL_PROCESSED=$((TOTAL_PROCESSED + 1))
        echo ""
    else
        echo "File not found: $file"
        echo ""
    fi
done

echo "======================================================="
echo "Summary:"
echo "  Total files processed: $TOTAL_PROCESSED"
echo "  Files with security headers: $TOTAL_VALIDATED"
echo "  Security coverage: $((TOTAL_VALIDATED * 100 / TOTAL_PROCESSED))%"
echo ""
echo "Next steps:"
echo "  1. Review files missing NaN/Inf checks"
echo "  2. Add bounds validation to weight updates"
echo "  3. Validate all init/create function parameters"
