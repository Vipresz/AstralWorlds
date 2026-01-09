/**
 * ML Feature Extractor for DensitySDF Training
 * 
 * This tool processes mesh files (OBJ) and point cloud files (TXT, NPZ) 
 * to generate training data for a regression forest that predicts the 
 * projection difference vector Δ_S(x).
 * 
 * Supported input formats:
 *   - OBJ mesh files (vertices and faces)
 *   - ShapeNet TXT files (x y z nx ny nz label per line)
 *   - ShapeNet NPZ files (NumPy archive with 'pc', 'sn' arrays)
 * 
 * Usage:
 *   ./ml_extractor <input_directory> <output_directory> [options]
 * 
 * Options:
 *   --shapenet            Process as ShapeNet directory structure
 *   --max-per-category N  Max files per category in ShapeNet mode (default: all)
 *   --num-features N      Number of random box features (default: 2000)
 *   --voxel-size F        Voxel size in world units (default: 0.02)
 *   --splat-sigma F       Gaussian splat sigma (default: 0.03)
 *   --samples-per-mesh N  Training samples per mesh (default: 50000)
 *   --sample-radius F     Max distance from surface to sample (default: 0.1)
 *   --seed N              Random seed for reproducibility (default: 42)
 *   --help                Show this help message
 * 
 * Example:
 *   # Process OBJ files
 *   ./ml_extractor ./meshes ./training_data --num-features 500
 * 
 *   # Process ShapeNet dataset
 *   ./ml_extractor ./shapenet_data ./training_data --shapenet --max-per-category 10
 * 
 * Output files:
 *   - box_features.csv           Box feature definitions
 *   - <filename>.bin             Binary training data per file
 *   - <filename>.csv             CSV sample for debugging
 *   - combined_training_data.bin All training data combined
 *   - combined_sample.csv        Sample of combined data
 * 
 * Binary format:
 *   Header:
 *     uint32_t numSamples
 *     uint32_t numFeatures
 *   Per sample:
 *     float[numFeatures] features (box-sum values)
 *     float[3] delta (x, y, z components of Δ_S)
 */

#include "FeatureExtractor.h"
#include <iostream>
#include <string>
#include <cstdlib>

void PrintUsage(const char* programName) {
    std::cout << "ML Feature Extractor for DensitySDF Training\n\n";
    std::cout << "Usage: " << programName << " <input_directory> <output_directory> [options]\n\n";
    std::cout << "Input formats supported:\n";
    std::cout << "  - OBJ mesh files\n";
    std::cout << "  - ShapeNet TXT point clouds (x y z nx ny nz label)\n";
    std::cout << "  - ShapeNet NPZ archives (with 'pc', 'sn' arrays)\n\n";
    std::cout << "Options:\n";
    std::cout << "  --shapenet            Process as ShapeNet directory structure\n";
    std::cout << "  --max-per-category N  Max files per category in ShapeNet mode\n";
    std::cout << "  --num-features N      Number of random box features (default: 2000)\n";
    std::cout << "  --voxel-size F        Voxel size in world units (default: 0.02)\n";
    std::cout << "  --splat-sigma F       Gaussian splat sigma (default: 0.03)\n";
    std::cout << "  --samples-per-mesh N  Training samples per file (default: 50000)\n";
    std::cout << "  --sample-radius F     Max distance from surface to sample (default: 0.1)\n";
    std::cout << "  --seed N              Random seed for reproducibility (default: 42)\n";
    std::cout << "  --help                Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName << " ./meshes ./output --num-features 500\n";
    std::cout << "  " << programName << " ./shapenet ./output --shapenet --max-per-category 10\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        PrintUsage(argv[0]);
        return 1;
    }
    
    std::string inputDir;
    std::string outputDir;
    MLExtractor::ExtractorConfig config;
    bool shapeNetMode = false;
    int maxPerCategory = -1;
    
    // Parse arguments
    int argIdx = 1;
    while (argIdx < argc) {
        std::string arg = argv[argIdx];
        
        if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            return 0;
        } else if (arg == "--shapenet") {
            shapeNetMode = true;
        } else if (arg == "--max-per-category" && argIdx + 1 < argc) {
            maxPerCategory = std::atoi(argv[++argIdx]);
        } else if (arg == "--num-features" && argIdx + 1 < argc) {
            config.numBoxFeatures = std::atoi(argv[++argIdx]);
        } else if (arg == "--voxel-size" && argIdx + 1 < argc) {
            config.voxelSize = std::atof(argv[++argIdx]);
        } else if (arg == "--splat-sigma" && argIdx + 1 < argc) {
            config.splatSigma = std::atof(argv[++argIdx]);
        } else if (arg == "--samples-per-mesh" && argIdx + 1 < argc) {
            config.samplesPerMesh = std::atoi(argv[++argIdx]);
        } else if (arg == "--sample-radius" && argIdx + 1 < argc) {
            config.sampleRadius = std::atof(argv[++argIdx]);
        } else if (arg == "--seed" && argIdx + 1 < argc) {
            config.randomSeed = std::atoi(argv[++argIdx]);
        } else if (arg[0] != '-') {
            // Positional argument
            if (inputDir.empty()) {
                inputDir = arg;
            } else if (outputDir.empty()) {
                outputDir = arg;
            } else {
                std::cerr << "Error: Unexpected argument: " << arg << "\n";
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            PrintUsage(argv[0]);
            return 1;
        }
        argIdx++;
    }
    
    if (inputDir.empty() || outputDir.empty()) {
        std::cerr << "Error: Input and output directories are required\n";
        PrintUsage(argv[0]);
        return 1;
    }
    
    std::cout << "========================================\n";
    std::cout << "ML Feature Extractor for DensitySDF\n";
    std::cout << "========================================\n";
    std::cout << "Input directory:  " << inputDir << "\n";
    std::cout << "Output directory: " << outputDir << "\n";
    std::cout << "Mode: " << (shapeNetMode ? "ShapeNet" : "Standard") << "\n";
    if (shapeNetMode && maxPerCategory > 0) {
        std::cout << "Max per category: " << maxPerCategory << "\n";
    }
    std::cout << "Configuration:\n";
    std::cout << "  Box features:     " << config.numBoxFeatures << "\n";
    std::cout << "  Voxel size:       " << config.voxelSize << "\n";
    std::cout << "  Splat sigma:      " << config.splatSigma << "\n";
    std::cout << "  Samples per file: " << config.samplesPerMesh << "\n";
    std::cout << "  Sample radius:    " << config.sampleRadius << "\n";
    std::cout << "  Random seed:      " << config.randomSeed << "\n";
    std::cout << "========================================\n\n";
    
    bool success;
    if (shapeNetMode) {
        success = MLExtractor::ProcessShapeNetDirectory(inputDir, outputDir, config, maxPerCategory);
    } else {
        success = MLExtractor::ProcessDirectory(inputDir, outputDir, config);
    }
    
    if (success) {
        std::cout << "\n========================================\n";
        std::cout << "Feature extraction completed successfully!\n";
        std::cout << "========================================\n";
        std::cout << "\nNext steps:\n";
        std::cout << "  1. Train model:  python training/train_forest.py --data " 
                  << outputDir << "/combined_sample.csv\n";
        std::cout << "  2. Export model: python training/export_model.py --model training/model/forest_model.joblib\n";
        return 0;
    } else {
        std::cerr << "\nFeature extraction failed. Check the input directory.\n";
        return 1;
    }
}
