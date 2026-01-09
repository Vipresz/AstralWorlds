#!/usr/bin/env python3
"""
Export trained Random Forest model to C++ readable format.

Random Forests don't have "weights and biases" like neural networks.
Instead, they have decision trees with:
- Split nodes: feature_index, threshold, left_child, right_child
- Leaf nodes: predicted values (delta_x, delta_y, delta_z)

This script exports the forest structure to:
1. Binary format (.bin) - efficient for C++ loading
2. Header format (.h) - can be compiled directly into C++

Usage:
    python export_model.py --model model/forest_model.joblib --output ../
"""

import argparse
import os
import struct
import numpy as np
import joblib


def extract_tree_structure(tree, tree_idx):
    """
    Extract the structure of a single decision tree.
    
    Returns a list of nodes, where each node is:
    - For split nodes: (is_leaf=0, feature_idx, threshold, left_child, right_child, 0, 0, 0)
    - For leaf nodes: (is_leaf=1, 0, 0, 0, 0, delta_x, delta_y, delta_z)
    """
    tree_ = tree.tree_
    nodes = []
    
    def traverse(node_id):
        if tree_.feature[node_id] == -2:  # Leaf node
            # Get prediction value (mean of training samples at this leaf)
            # For multi-output regression, values has shape [n_nodes, n_outputs, 1]
            values = tree_.value[node_id]
            if len(values.shape) == 3:
                delta = values[:, 0].flatten()  # Shape: (3,) for 3 outputs
            else:
                delta = values.flatten()
            
            nodes.append({
                'is_leaf': True,
                'feature_index': 0,
                'threshold': 0.0,
                'left_child': 0,
                'right_child': 0,
                'delta': delta.tolist() if len(delta) >= 3 else [delta[0], 0, 0]
            })
        else:  # Split node
            left_child = tree_.children_left[node_id]
            right_child = tree_.children_right[node_id]
            
            # Record current node
            current_idx = len(nodes)
            nodes.append({
                'is_leaf': False,
                'feature_index': int(tree_.feature[node_id]),
                'threshold': float(tree_.threshold[node_id]),
                'left_child': 0,  # Will be filled later
                'right_child': 0,  # Will be filled later
                'delta': [0.0, 0.0, 0.0]
            })
            
            # Traverse children
            nodes[current_idx]['left_child'] = len(nodes)
            traverse(left_child)
            nodes[current_idx]['right_child'] = len(nodes)
            traverse(right_child)
    
    traverse(0)
    return nodes


def export_binary(forest, box_features_path, output_path):
    """
    Export forest to binary format for C++ loading.
    
    Binary format:
    Header:
        uint32_t num_trees
        uint32_t num_features (box features count)
    
    Per tree:
        uint32_t num_nodes
        Per node:
            uint8_t is_leaf
            uint32_t feature_index (for split nodes)
            float threshold (for split nodes)
            uint32_t left_child (for split nodes)
            uint32_t right_child (for split nodes)
            float delta_x, delta_y, delta_z (for leaf nodes)
    
    Box features (at the end):
        Per feature:
            int32_t offset_x, offset_y, offset_z
            int32_t halfsize_x, halfsize_y, halfsize_z
    """
    print(f"\nExporting to binary format: {output_path}")
    
    # Load box features if provided
    box_features = []
    if box_features_path and os.path.exists(box_features_path):
        print(f"  Loading box features from {box_features_path}")
        with open(box_features_path, 'r') as f:
            for line in f:
                if line.startswith('#'):
                    continue
                parts = line.strip().split(',')
                if len(parts) == 6:
                    box_features.append([int(x) for x in parts])
    
    with open(output_path, 'wb') as f:
        # Header
        num_trees = len(forest.estimators_)
        num_box_features = len(box_features)
        f.write(struct.pack('I', num_trees))
        f.write(struct.pack('I', num_box_features))
        
        print(f"  Writing {num_trees} trees...")
        
        # Write each tree
        for tree_idx, tree in enumerate(forest.estimators_):
            nodes = extract_tree_structure(tree, tree_idx)
            
            # Write number of nodes
            f.write(struct.pack('I', len(nodes)))
            
            # Write nodes
            for node in nodes:
                f.write(struct.pack('B', 1 if node['is_leaf'] else 0))
                f.write(struct.pack('I', node['feature_index']))
                f.write(struct.pack('f', node['threshold']))
                f.write(struct.pack('I', node['left_child']))
                f.write(struct.pack('I', node['right_child']))
                f.write(struct.pack('3f', *node['delta']))
        
        # Write box features
        print(f"  Writing {num_box_features} box features...")
        for bf in box_features:
            f.write(struct.pack('6i', *bf))
    
    # Calculate file size
    file_size = os.path.getsize(output_path)
    print(f"  Binary file size: {file_size / 1024:.1f} KB")


def export_header(forest, box_features_path, output_path):
    """
    Export forest as a C++ header file.
    This can be compiled directly into the application.
    """
    print(f"\nExporting to C++ header: {output_path}")
    
    # Load box features
    box_features = []
    if box_features_path and os.path.exists(box_features_path):
        with open(box_features_path, 'r') as f:
            for line in f:
                if line.startswith('#'):
                    continue
                parts = line.strip().split(',')
                if len(parts) == 6:
                    box_features.append([int(x) for x in parts])
    
    with open(output_path, 'w') as f:
        f.write("// Auto-generated DensitySDF Random Forest Model\n")
        f.write("// Do not edit manually - regenerate with export_model.py\n")
        f.write("#pragma once\n\n")
        f.write("#include <cstdint>\n")
        f.write("#include <array>\n")
        f.write("#include <vector>\n\n")
        f.write("namespace DensitySDF {\n\n")
        
        # Constants
        num_trees = len(forest.estimators_)
        num_features = len(box_features)
        f.write(f"constexpr uint32_t NUM_TREES = {num_trees};\n")
        f.write(f"constexpr uint32_t NUM_FEATURES = {num_features};\n\n")
        
        # Node structure
        f.write("struct TreeNode {\n")
        f.write("    bool isLeaf;\n")
        f.write("    uint32_t featureIndex;\n")
        f.write("    float threshold;\n")
        f.write("    uint32_t leftChild;\n")
        f.write("    uint32_t rightChild;\n")
        f.write("    float deltaX, deltaY, deltaZ;\n")
        f.write("};\n\n")
        
        # Box feature structure
        f.write("struct BoxFeature {\n")
        f.write("    int32_t offsetX, offsetY, offsetZ;\n")
        f.write("    int32_t halfSizeX, halfSizeY, halfSizeZ;\n")
        f.write("};\n\n")
        
        # Box features data - use regular array instead of std::array with double braces
        f.write("inline const BoxFeature BOX_FEATURES[] = {\n")
        for i, bf in enumerate(box_features):
            f.write(f"    {{{bf[0]}, {bf[1]}, {bf[2]}, {bf[3]}, {bf[4]}, {bf[5]}}}")
            if i < len(box_features) - 1:
                f.write(",")
            f.write("\n")
        f.write("};\n\n")
        
        # Tree data
        f.write("// Tree node data - stored as flat arrays per tree\n")
        
        # Collect all trees
        all_trees = []
        for tree_idx, tree in enumerate(forest.estimators_):
            nodes = extract_tree_structure(tree, tree_idx)
            all_trees.append(nodes)
        
        # Write tree sizes
        f.write("inline const uint32_t TREE_SIZES[] = {\n    ")
        for i, nodes in enumerate(all_trees):
            f.write(f"{len(nodes)}")
            if i < len(all_trees) - 1:
                f.write(", ")
            if (i + 1) % 10 == 0:
                f.write("\n    ")
        f.write("\n};\n\n")
        
        # Write tree nodes
        for tree_idx, nodes in enumerate(all_trees):
            f.write(f"inline const TreeNode TREE_{tree_idx}[] = {{\n")
            for node in nodes:
                is_leaf = "true" if node['is_leaf'] else "false"
                f.write(f"    {{{is_leaf}, {node['feature_index']}, {node['threshold']:.8f}f, ")
                f.write(f"{node['left_child']}, {node['right_child']}, ")
                f.write(f"{node['delta'][0]:.8f}f, {node['delta'][1]:.8f}f, {node['delta'][2]:.8f}f}},\n")
            f.write("};\n\n")
        
        # Tree pointer array
        f.write("inline const TreeNode* TREES[] = {\n")
        for i in range(len(all_trees)):
            f.write(f"    TREE_{i}")
            if i < len(all_trees) - 1:
                f.write(",")
            f.write("\n")
        f.write("};\n\n")
        
        # Inference function
        f.write("// Predict delta for a single query point given extracted features\n")
        f.write("inline void PredictDelta(const float* features, float& deltaX, float& deltaY, float& deltaZ) {\n")
        f.write("    deltaX = deltaY = deltaZ = 0.0f;\n")
        f.write("    \n")
        f.write("    for (uint32_t t = 0; t < NUM_TREES; t++) {\n")
        f.write("        const TreeNode* tree = TREES[t];\n")
        f.write("        uint32_t nodeIdx = 0;\n")
        f.write("        \n")
        f.write("        while (!tree[nodeIdx].isLeaf) {\n")
        f.write("            if (features[tree[nodeIdx].featureIndex] < tree[nodeIdx].threshold) {\n")
        f.write("                nodeIdx = tree[nodeIdx].leftChild;\n")
        f.write("            } else {\n")
        f.write("                nodeIdx = tree[nodeIdx].rightChild;\n")
        f.write("            }\n")
        f.write("        }\n")
        f.write("        \n")
        f.write("        deltaX += tree[nodeIdx].deltaX;\n")
        f.write("        deltaY += tree[nodeIdx].deltaY;\n")
        f.write("        deltaZ += tree[nodeIdx].deltaZ;\n")
        f.write("    }\n")
        f.write("    \n")
        f.write("    // Average over all trees\n")
        f.write("    float invTrees = 1.0f / static_cast<float>(NUM_TREES);\n")
        f.write("    deltaX *= invTrees;\n")
        f.write("    deltaY *= invTrees;\n")
        f.write("    deltaZ *= invTrees;\n")
        f.write("}\n\n")
        
        f.write("} // namespace DensitySDF\n")
    
    print(f"  Header file generated with {num_trees} trees")


def export_compact_binary(forest, box_features_path, output_path):
    """
    Export forest to a compact binary format optimized for loading.
    
    Format:
        Header (16 bytes):
            char[4] magic = "DSDF"
            uint32_t version = 1
            uint32_t num_trees
            uint32_t num_features
        
        Box Features:
            Per feature (24 bytes):
                int32_t offset[3]
                int32_t halfSize[3]
        
        Tree offsets (to allow random access):
            uint32_t tree_node_counts[num_trees]
        
        Tree nodes:
            Per tree, per node (32 bytes):
                uint8_t is_leaf
                uint8_t padding[3]
                uint32_t feature_index
                float threshold
                uint32_t left_child
                uint32_t right_child
                float delta[3]
    """
    print(f"\nExporting to compact binary: {output_path}")
    
    # Load box features
    box_features = []
    if box_features_path and os.path.exists(box_features_path):
        with open(box_features_path, 'r') as f:
            for line in f:
                if line.startswith('#'):
                    continue
                parts = line.strip().split(',')
                if len(parts) == 6:
                    box_features.append([int(x) for x in parts])
    
    with open(output_path, 'wb') as f:
        # Magic and version
        f.write(b'DSDF')
        f.write(struct.pack('I', 1))  # Version
        
        num_trees = len(forest.estimators_)
        num_features = len(box_features)
        f.write(struct.pack('I', num_trees))
        f.write(struct.pack('I', num_features))
        
        # Box features
        for bf in box_features:
            f.write(struct.pack('6i', *bf))
        
        # Extract all trees first to get node counts
        all_trees = []
        for tree_idx, tree in enumerate(forest.estimators_):
            nodes = extract_tree_structure(tree, tree_idx)
            all_trees.append(nodes)
        
        # Tree node counts
        for nodes in all_trees:
            f.write(struct.pack('I', len(nodes)))
        
        # Tree nodes
        for nodes in all_trees:
            for node in nodes:
                f.write(struct.pack('B', 1 if node['is_leaf'] else 0))
                f.write(struct.pack('3B', 0, 0, 0))  # Padding
                f.write(struct.pack('I', node['feature_index']))
                f.write(struct.pack('f', node['threshold']))
                f.write(struct.pack('I', node['left_child']))
                f.write(struct.pack('I', node['right_child']))
                f.write(struct.pack('3f', *node['delta']))
    
    file_size = os.path.getsize(output_path)
    print(f"  Compact binary size: {file_size / 1024:.1f} KB")


def main():
    parser = argparse.ArgumentParser(description='Export Random Forest for C++ use')
    parser.add_argument('--model', required=True, help='Path to trained model (.joblib)')
    parser.add_argument('--box-features', default='../test_output/box_features.csv',
                        help='Path to box features CSV')
    parser.add_argument('--output', default='../', help='Output directory')
    parser.add_argument('--format', choices=['binary', 'header', 'compact', 'all'], 
                        default='all', help='Export format')
    
    args = parser.parse_args()
    
    # Load model
    print(f"Loading model from {args.model}...")
    forest = joblib.load(args.model)
    print(f"  Loaded forest with {len(forest.estimators_)} trees")
    
    os.makedirs(args.output, exist_ok=True)
    
    # Export
    if args.format in ['binary', 'all']:
        export_binary(forest, args.box_features, 
                     os.path.join(args.output, 'forest_model.bin'))
    
    if args.format in ['header', 'all']:
        export_header(forest, args.box_features,
                     os.path.join(args.output, 'DensitySDFModel.h'))
    
    if args.format in ['compact', 'all']:
        export_compact_binary(forest, args.box_features,
                             os.path.join(args.output, 'forest_model_compact.bin'))
    
    print("\n" + "=" * 50)
    print("Export complete!")
    print("\nTo use in C++:")
    print("  Option 1: Include DensitySDFModel.h directly")
    print("  Option 2: Load forest_model_compact.bin at runtime")
    print("=" * 50)


if __name__ == '__main__':
    main()
