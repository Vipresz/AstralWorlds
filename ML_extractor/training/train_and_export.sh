#!/bin/bash
# Train and export DensitySDF model

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Default values
DATA_PATH="../test_output/combined_sample.csv"
BOX_FEATURES="../test_output/box_features.csv"
OUTPUT_DIR=".."
MODEL_DIR="model"
N_ESTIMATORS=50
MAX_DEPTH=20
BINARY=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --data)
            DATA_PATH="$2"
            shift 2
            ;;
        --binary)
            BINARY=true
            shift
            ;;
        --output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --n-estimators)
            N_ESTIMATORS="$2"
            shift 2
            ;;
        --max-depth)
            MAX_DEPTH="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

echo "=========================================="
echo "DensitySDF Model Training & Export"
echo "=========================================="
echo "Data: $DATA_PATH"
echo "Trees: $N_ESTIMATORS"
echo "Max depth: $MAX_DEPTH"
echo "=========================================="

# Check for Python
if ! command -v python3 &> /dev/null; then
    echo "Error: python3 not found"
    exit 1
fi

# Install dependencies if needed
if ! python3 -c "import sklearn" &> /dev/null; then
    echo "Installing dependencies..."
    pip3 install -r requirements.txt
fi

# Train model
echo ""
echo "Step 1: Training model..."
BINARY_FLAG=""
if [ "$BINARY" = true ]; then
    BINARY_FLAG="--binary"
fi

python3 train_forest.py \
    --data "$DATA_PATH" \
    $BINARY_FLAG \
    --output "$MODEL_DIR" \
    --n-estimators "$N_ESTIMATORS" \
    --max-depth "$MAX_DEPTH"

# Export model
echo ""
echo "Step 2: Exporting model..."
python3 export_model.py \
    --model "$MODEL_DIR/forest_model.joblib" \
    --box-features "$BOX_FEATURES" \
    --output "$OUTPUT_DIR" \
    --format all

echo ""
echo "=========================================="
echo "Done! Generated files:"
echo "  $OUTPUT_DIR/forest_model_compact.bin  (for runtime loading)"
echo "  $OUTPUT_DIR/DensitySDFModel.h         (for compiled-in model)"
echo "=========================================="
