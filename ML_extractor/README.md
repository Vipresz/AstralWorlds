# ShapeNet Training Pipeline

## Directory Structure

```
shapenet/
├── shapenet_input/      # ShapeNet point cloud data (TXT files)
│   ├── 02691156/        # Airplane
│   ├── 02958343/        # Car
│   ├── 03001627/        # Chair
│   └── ...              # Other categories
├── shapenet_output/     # Generated training data (created by extractor)
└── README.md
```

## Quick Start

### 1. Build the Feature Extractor

```bash
cd /Users/rgrigorii/Desktop/projects/thegame/ML_extractor
mkdir -p build && cd build
cmake .. && make -j8
```

### 2. Extract Features from ShapeNet

```bash
cd /Users/rgrigorii/Desktop/projects/thegame/ML_extractor

# Process all categories (limit 10 files per category for testing)
./build/bin/ml_extractor shapenet/shapenet_input shapenet/shapenet_output \
    --shapenet \
    --max-per-category 10 \
    --num-features 500 \
    --samples-per-mesh 10000

# For full training (no limit, more features):
./build/bin/ml_extractor shapenet/shapenet_input shapenet/shapenet_output \
    --shapenet \
    --num-features 1000 \
    --samples-per-mesh 20000
```

**Options:**
- `--shapenet` - Process ShapeNet directory structure (category/instance)
- `--max-per-category N` - Limit files per category (omit for all)
- `--num-features N` - Number of box features (default: 2000)
- `--samples-per-mesh N` - Training samples per file (default: 50000)

### 3. Train the Model

```bash
cd /Users/rgrigorii/Desktop/projects/thegame/ML_extractor/training

# Install Python dependencies (one-time)
pip3 install -r requirements.txt

# Train on extracted data
python3 train_forest.py \
    --data ../shapenet/shapenet_output/combined_sample.csv \
    --output model/ \
    --n-estimators 50 \
    --max-depth 20
```

**Training options:**
- `--n-estimators N` - Number of trees (more = better but slower, default: 50)
- `--max-depth N` - Max tree depth (default: 20)
- `--binary` - Use binary format instead of CSV (faster for large datasets)

### 4. Export Model for C++

```bash
python3 export_model.py \
    --model model/forest_model.joblib \
    --box-features ../shapenet/shapenet_output/box_features.csv \
    --output ../
```

This creates:
- `DensitySDFModel.h` - Compiled-in C++ header
- `forest_model_compact.bin` - Runtime loadable binary

## Full Pipeline (One Command)

```bash
cd /Users/rgrigorii/Desktop/projects/thegame/ML_extractor

# Extract + Train + Export
./build/bin/ml_extractor shapenet/shapenet_input shapenet/shapenet_output \
    --shapenet --max-per-category 20 --num-features 500 && \
cd training && \
python3 train_forest.py --data ../shapenet/shapenet_output/combined_sample.csv --output model/ && \
python3 export_model.py --model model/forest_model.joblib --box-features ../shapenet/shapenet_output/box_features.csv --output ../
```

## Output Files

After running the pipeline:

```
shapenet_output/
├── box_features.csv              # Box feature definitions
├── combined_training_data.bin    # All samples (binary)
├── combined_sample.csv           # Sample for training (CSV)
└── <category_id>/                # Per-category data
    ├── training_data.bin
    └── sample.csv

ML_extractor/
├── DensitySDFModel.h             # Include in C++ project
├── forest_model_compact.bin      # Load at runtime
└── training/model/
    ├── forest_model.joblib       # Python model
    └── metrics.txt               # Training metrics
```

## Using the Trained Model

See `ForestInference.h` for C++ integration examples.
