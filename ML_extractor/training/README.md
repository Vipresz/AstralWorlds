# DensitySDF Random Forest Training

This directory contains scripts for training a Random Forest model to predict projection difference vectors Δ_S(x) from box-sum density features Φ(x).

## Quick Start

```bash
# 1. Install dependencies
pip install -r requirements.txt

# 2. Train model on extracted features
python train_forest.py --data ../test_output/combined_sample.csv --output model/

# 3. Export model for C++ use
python export_model.py --model model/forest_model.joblib --output ../
```

## Files Generated

After training and export:

```
model/
├── forest_model.joblib    # Python sklearn model
└── metrics.txt            # Training metrics

../
├── forest_model.bin       # Binary format (legacy)
├── forest_model_compact.bin  # Optimized binary for C++ loading
└── DensitySDFModel.h      # C++ header (compiled-in model)
```

## Using in C++

### Option 1: Compiled-in Model (Fastest startup, larger binary)

```cpp
#include "DensitySDFModel.h"

// Features already extracted from density field
std::vector<float> features(DensitySDF::NUM_FEATURES);
// ... extract features using DensitySDF::BOX_FEATURES ...

float deltaX, deltaY, deltaZ;
DensitySDF::PredictDelta(features.data(), deltaX, deltaY, deltaZ);
```

### Option 2: Runtime Loading (Slower startup, smaller binary)

```cpp
#include "ForestInference.h"

DensitySDF::ForestModel model;
if (!model.LoadFromFile("forest_model_compact.bin")) {
    // Handle error
}

// Extract features from density field
auto features = model.ExtractFeatures(integralVolume, dimX, dimY, dimZ, x, y, z);

float deltaX, deltaY, deltaZ;
model.Predict(features, deltaX, deltaY, deltaZ);

// Use delta to compute SDF:
// unsigned_distance = sqrt(deltaX*deltaX + deltaY*deltaY + deltaZ*deltaZ)
// signed_distance = unsigned_distance * sign(dot(delta, normal))
```

## Training Options

```bash
python train_forest.py --help

Options:
  --data          Path to training data (CSV or binary)
  --binary        Data is in binary format
  --output        Output directory for model
  --n-estimators  Number of trees (default: 50)
  --max-depth     Max tree depth (default: 20)
  --min-samples-leaf  Min samples per leaf (default: 5)
```

## Export Options

```bash
python export_model.py --help

Options:
  --model         Path to trained .joblib model
  --box-features  Path to box_features.csv
  --output        Output directory
  --format        binary, header, compact, or all
```

## Model Architecture

The Random Forest predicts:
- **Input**: Φ(x) = [Ω(x+B₁), ..., Ω(x+Bₘ)] - M box-sum features
- **Output**: Δ_S(x) = (delta_x, delta_y, delta_z) - projection difference vector

The projection difference tells us:
- **Distance to surface**: ||Δ_S(x)|| 
- **Direction to surface**: -Δ_S(x) / ||Δ_S(x)||

## Generating More Training Data

To improve model quality, train on more meshes:

```bash
# 1. Place OBJ files in a directory
mkdir meshes/
cp *.obj meshes/

# 2. Extract features
../build/bin/ml_extractor meshes/ training_data/ --num-features 2000 --samples-per-mesh 100000

# 3. Train on combined data
python train_forest.py --data training_data/combined_training_data.bin --binary --output model/

# 4. Export
python export_model.py --model model/forest_model.joblib
```
