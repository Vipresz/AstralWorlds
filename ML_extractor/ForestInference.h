#pragma once
/**
 * DensitySDF Random Forest Inference
 * 
 * This header provides functionality to load and run inference with
 * a trained Random Forest model for predicting projection difference
 * vectors Δ_S(x) from box-sum density features Φ(x).
 * 
 * Usage:
 *   1. Train model with Python: python train_forest.py --data data.csv
 *   2. Export model: python export_model.py --model model/forest_model.joblib
 *   3. Load in C++:
 *      DensitySDF::ForestModel model;
 *      model.LoadFromFile("forest_model_compact.bin");
 *      
 *   4. Extract features and predict:
 *      std::vector<float> features = ExtractBoxFeatures(densityField, x, y, z, model.GetBoxFeatures());
 *      glm::vec3 delta = model.Predict(features);
 */

#include <vector>
#include <string>
#include <fstream>
#include <cstdint>
#include <cmath>
#include <stdexcept>

#ifdef USE_GLM
#include <glm/glm.hpp>
#endif

namespace DensitySDF {

// Box feature definition
struct BoxFeature {
    int32_t offsetX, offsetY, offsetZ;
    int32_t halfSizeX, halfSizeY, halfSizeZ;
};

// Tree node
struct TreeNode {
    bool isLeaf;
    uint32_t featureIndex;
    float threshold;
    uint32_t leftChild;
    uint32_t rightChild;
    float deltaX, deltaY, deltaZ;
};

// Single decision tree
struct DecisionTree {
    std::vector<TreeNode> nodes;
    
    void Predict(const float* features, float& dx, float& dy, float& dz) const {
        uint32_t nodeIdx = 0;
        
        while (!nodes[nodeIdx].isLeaf) {
            if (features[nodes[nodeIdx].featureIndex] < nodes[nodeIdx].threshold) {
                nodeIdx = nodes[nodeIdx].leftChild;
            } else {
                nodeIdx = nodes[nodeIdx].rightChild;
            }
        }
        
        dx = nodes[nodeIdx].deltaX;
        dy = nodes[nodeIdx].deltaY;
        dz = nodes[nodeIdx].deltaZ;
    }
};

// Random Forest model
class ForestModel {
public:
    ForestModel() = default;
    
    /**
     * Load model from compact binary file.
     * 
     * Format:
     *   Header: "DSDF" + version + num_trees + num_features
     *   Box features: offset[3] + halfSize[3] per feature
     *   Tree node counts: uint32 per tree
     *   Tree nodes: isLeaf + padding + featureIndex + threshold + children + delta
     */
    bool LoadFromFile(const std::string& filepath) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        
        // Read magic
        char magic[4];
        file.read(magic, 4);
        if (magic[0] != 'D' || magic[1] != 'S' || magic[2] != 'D' || magic[3] != 'F') {
            return false;
        }
        
        // Read version
        uint32_t version;
        file.read(reinterpret_cast<char*>(&version), 4);
        if (version != 1) {
            return false;
        }
        
        // Read counts
        uint32_t numTrees, numFeatures;
        file.read(reinterpret_cast<char*>(&numTrees), 4);
        file.read(reinterpret_cast<char*>(&numFeatures), 4);
        
        // Read box features
        m_boxFeatures.resize(numFeatures);
        for (uint32_t i = 0; i < numFeatures; i++) {
            file.read(reinterpret_cast<char*>(&m_boxFeatures[i].offsetX), 4);
            file.read(reinterpret_cast<char*>(&m_boxFeatures[i].offsetY), 4);
            file.read(reinterpret_cast<char*>(&m_boxFeatures[i].offsetZ), 4);
            file.read(reinterpret_cast<char*>(&m_boxFeatures[i].halfSizeX), 4);
            file.read(reinterpret_cast<char*>(&m_boxFeatures[i].halfSizeY), 4);
            file.read(reinterpret_cast<char*>(&m_boxFeatures[i].halfSizeZ), 4);
        }
        
        // Read tree node counts
        std::vector<uint32_t> nodeCounts(numTrees);
        for (uint32_t t = 0; t < numTrees; t++) {
            file.read(reinterpret_cast<char*>(&nodeCounts[t]), 4);
        }
        
        // Read trees
        m_trees.resize(numTrees);
        for (uint32_t t = 0; t < numTrees; t++) {
            m_trees[t].nodes.resize(nodeCounts[t]);
            
            for (uint32_t n = 0; n < nodeCounts[t]; n++) {
                uint8_t isLeaf;
                uint8_t padding[3];
                file.read(reinterpret_cast<char*>(&isLeaf), 1);
                file.read(reinterpret_cast<char*>(padding), 3);
                
                m_trees[t].nodes[n].isLeaf = (isLeaf != 0);
                file.read(reinterpret_cast<char*>(&m_trees[t].nodes[n].featureIndex), 4);
                file.read(reinterpret_cast<char*>(&m_trees[t].nodes[n].threshold), 4);
                file.read(reinterpret_cast<char*>(&m_trees[t].nodes[n].leftChild), 4);
                file.read(reinterpret_cast<char*>(&m_trees[t].nodes[n].rightChild), 4);
                file.read(reinterpret_cast<char*>(&m_trees[t].nodes[n].deltaX), 4);
                file.read(reinterpret_cast<char*>(&m_trees[t].nodes[n].deltaY), 4);
                file.read(reinterpret_cast<char*>(&m_trees[t].nodes[n].deltaZ), 4);
            }
        }
        
        m_loaded = true;
        return true;
    }
    
    /**
     * Predict projection difference vector from features.
     * 
     * @param features Box-sum features extracted from density field
     * @param deltaX Output: X component of Δ_S(x)
     * @param deltaY Output: Y component of Δ_S(x)
     * @param deltaZ Output: Z component of Δ_S(x)
     */
    void Predict(const float* features, float& deltaX, float& deltaY, float& deltaZ) const {
        deltaX = deltaY = deltaZ = 0.0f;
        
        for (const auto& tree : m_trees) {
            float dx, dy, dz;
            tree.Predict(features, dx, dy, dz);
            deltaX += dx;
            deltaY += dy;
            deltaZ += dz;
        }
        
        // Average over all trees
        float invTrees = 1.0f / static_cast<float>(m_trees.size());
        deltaX *= invTrees;
        deltaY *= invTrees;
        deltaZ *= invTrees;
    }
    
    void Predict(const std::vector<float>& features, float& deltaX, float& deltaY, float& deltaZ) const {
        Predict(features.data(), deltaX, deltaY, deltaZ);
    }
    
#ifdef USE_GLM
    glm::vec3 Predict(const std::vector<float>& features) const {
        float dx, dy, dz;
        Predict(features.data(), dx, dy, dz);
        return glm::vec3(dx, dy, dz);
    }
#endif
    
    /**
     * Get box features for feature extraction.
     */
    const std::vector<BoxFeature>& GetBoxFeatures() const {
        return m_boxFeatures;
    }
    
    /**
     * Extract features from a density field at a given voxel position.
     * 
     * @param densityField Pointer to density values
     * @param integralVolume Pointer to integral volume for O(1) box queries
     * @param dimX, dimY, dimZ Grid dimensions
     * @param x, y, z Query voxel position
     * @return Feature vector
     */
    std::vector<float> ExtractFeatures(
        const float* integralVolume,
        int dimX, int dimY, int dimZ,
        int x, int y, int z
    ) const {
        std::vector<float> features(m_boxFeatures.size());
        
        auto index = [dimX, dimY](int i, int j, int k) {
            return i + j * dimX + k * dimX * dimY;
        };
        
        auto boxSum = [&](int x0, int y0, int z0, int x1, int y1, int z1) -> float {
            // Clamp
            x0 = std::max(0, std::min(x0, dimX - 1));
            y0 = std::max(0, std::min(y0, dimY - 1));
            z0 = std::max(0, std::min(z0, dimZ - 1));
            x1 = std::max(0, std::min(x1, dimX - 1));
            y1 = std::max(0, std::min(y1, dimY - 1));
            z1 = std::max(0, std::min(z1, dimZ - 1));
            
            float sum = integralVolume[index(x1, y1, z1)];
            
            if (x0 > 0) sum -= integralVolume[index(x0-1, y1, z1)];
            if (y0 > 0) sum -= integralVolume[index(x1, y0-1, z1)];
            if (z0 > 0) sum -= integralVolume[index(x1, y1, z0-1)];
            
            if (x0 > 0 && y0 > 0) sum += integralVolume[index(x0-1, y0-1, z1)];
            if (x0 > 0 && z0 > 0) sum += integralVolume[index(x0-1, y1, z0-1)];
            if (y0 > 0 && z0 > 0) sum += integralVolume[index(x1, y0-1, z0-1)];
            
            if (x0 > 0 && y0 > 0 && z0 > 0) sum -= integralVolume[index(x0-1, y0-1, z0-1)];
            
            return sum;
        };
        
        for (size_t i = 0; i < m_boxFeatures.size(); i++) {
            const auto& bf = m_boxFeatures[i];
            
            int x0 = x + bf.offsetX - bf.halfSizeX;
            int y0 = y + bf.offsetY - bf.halfSizeY;
            int z0 = z + bf.offsetZ - bf.halfSizeZ;
            int x1 = x + bf.offsetX + bf.halfSizeX;
            int y1 = y + bf.offsetY + bf.halfSizeY;
            int z1 = z + bf.offsetZ + bf.halfSizeZ;
            
            features[i] = boxSum(x0, y0, z0, x1, y1, z1);
        }
        
        return features;
    }
    
    bool IsLoaded() const { return m_loaded; }
    size_t GetNumTrees() const { return m_trees.size(); }
    size_t GetNumFeatures() const { return m_boxFeatures.size(); }
    
private:
    std::vector<DecisionTree> m_trees;
    std::vector<BoxFeature> m_boxFeatures;
    bool m_loaded = false;
};

} // namespace DensitySDF
