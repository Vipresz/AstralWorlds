#!/usr/bin/env python3
"""
Train a Random Forest Regressor to predict projection difference vectors Δ_S(x)
from box-sum density features Φ(x).

This implements the learning component from the DensitySDF paper:
- Input: Φ(x) = [Ω(x+B₁), ..., Ω(x+Bₘ)] - box-sum features
- Output: Δ_S(x) = x - π_S(x) - projection difference vector

Usage:
    python train_forest.py --data ../test_output/combined_sample.csv --output model/
    python train_forest.py --data ../test_output/combined_training_data.bin --binary --output model/
"""

import argparse
import os
import struct
import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestRegressor
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_squared_error, r2_score
import joblib
from tqdm import tqdm


def load_csv_data(filepath):
    """Load training data from CSV file."""
    print(f"Loading CSV data from {filepath}...")
    df = pd.read_csv(filepath)
    
    # Extract feature columns (f0, f1, f2, ...)
    feature_cols = [col for col in df.columns if col.startswith('f')]
    X = df[feature_cols].values
    
    # Extract target columns (delta_x, delta_y, delta_z)
    y = df[['delta_x', 'delta_y', 'delta_z']].values
    
    print(f"  Loaded {X.shape[0]} samples with {X.shape[1]} features")
    return X, y


def load_binary_data(filepath):
    """Load training data from binary file."""
    print(f"Loading binary data from {filepath}...")
    
    with open(filepath, 'rb') as f:
        # Read header
        num_samples = struct.unpack('I', f.read(4))[0]
        num_features = struct.unpack('I', f.read(4))[0]
        
        print(f"  Header: {num_samples} samples, {num_features} features")
        
        X = np.zeros((num_samples, num_features), dtype=np.float32)
        y = np.zeros((num_samples, 3), dtype=np.float32)
        
        for i in tqdm(range(num_samples), desc="  Loading samples"):
            # Read features
            features = struct.unpack(f'{num_features}f', f.read(4 * num_features))
            X[i] = features
            
            # Read delta (3 floats)
            delta = struct.unpack('3f', f.read(12))
            y[i] = delta
    
    print(f"  Loaded {X.shape[0]} samples with {X.shape[1]} features")
    return X, y


def train_forest(X, y, n_estimators=50, max_depth=20, min_samples_leaf=5, n_jobs=-1):
    """Train Random Forest regressor."""
    print(f"\nTraining Random Forest...")
    print(f"  n_estimators: {n_estimators}")
    print(f"  max_depth: {max_depth}")
    print(f"  min_samples_leaf: {min_samples_leaf}")
    
    # Split data
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42
    )
    print(f"  Training samples: {len(X_train)}")
    print(f"  Test samples: {len(X_test)}")
    
    # Train forest
    forest = RandomForestRegressor(
        n_estimators=n_estimators,
        max_depth=max_depth,
        min_samples_leaf=min_samples_leaf,
        n_jobs=n_jobs,
        random_state=42,
        verbose=1
    )
    
    forest.fit(X_train, y_train)
    
    # Evaluate
    print("\nEvaluating model...")
    y_pred = forest.predict(X_test)
    
    mse = mean_squared_error(y_test, y_pred)
    rmse = np.sqrt(mse)
    r2 = r2_score(y_test, y_pred)
    
    # Per-component errors
    mse_x = mean_squared_error(y_test[:, 0], y_pred[:, 0])
    mse_y = mean_squared_error(y_test[:, 1], y_pred[:, 1])
    mse_z = mean_squared_error(y_test[:, 2], y_pred[:, 2])
    
    # Distance error (comparing ||Δ_predicted|| vs ||Δ_true||)
    dist_true = np.linalg.norm(y_test, axis=1)
    dist_pred = np.linalg.norm(y_pred, axis=1)
    dist_error = np.mean(np.abs(dist_true - dist_pred))
    
    print(f"\n  Overall MSE: {mse:.6f}")
    print(f"  Overall RMSE: {rmse:.6f}")
    print(f"  Overall R²: {r2:.4f}")
    print(f"  MSE per component: X={mse_x:.6f}, Y={mse_y:.6f}, Z={mse_z:.6f}")
    print(f"  Mean distance error: {dist_error:.6f}")
    
    return forest, {
        'mse': mse,
        'rmse': rmse,
        'r2': r2,
        'mse_x': mse_x,
        'mse_y': mse_y,
        'mse_z': mse_z,
        'dist_error': dist_error
    }


def save_model(forest, output_dir, metrics):
    """Save trained model and metrics."""
    os.makedirs(output_dir, exist_ok=True)
    
    # Save sklearn model (for Python use)
    model_path = os.path.join(output_dir, 'forest_model.joblib')
    joblib.dump(forest, model_path)
    print(f"\nSaved sklearn model to {model_path}")
    
    # Save metrics
    metrics_path = os.path.join(output_dir, 'metrics.txt')
    with open(metrics_path, 'w') as f:
        f.write("DensitySDF Random Forest Training Metrics\n")
        f.write("=" * 50 + "\n\n")
        for key, value in metrics.items():
            f.write(f"{key}: {value:.6f}\n")
    print(f"Saved metrics to {metrics_path}")
    
    return model_path


def main():
    parser = argparse.ArgumentParser(description='Train Random Forest for DensitySDF')
    parser.add_argument('--data', required=True, help='Path to training data (CSV or binary)')
    parser.add_argument('--binary', action='store_true', help='Data is in binary format')
    parser.add_argument('--output', default='model/', help='Output directory for model')
    parser.add_argument('--n-estimators', type=int, default=50, help='Number of trees')
    parser.add_argument('--max-depth', type=int, default=20, help='Max tree depth')
    parser.add_argument('--min-samples-leaf', type=int, default=5, help='Min samples per leaf')
    parser.add_argument('--n-jobs', type=int, default=-1, help='Number of parallel jobs')
    
    args = parser.parse_args()
    
    # Load data
    if args.binary:
        X, y = load_binary_data(args.data)
    else:
        X, y = load_csv_data(args.data)
    
    # Train
    forest, metrics = train_forest(
        X, y,
        n_estimators=args.n_estimators,
        max_depth=args.max_depth,
        min_samples_leaf=args.min_samples_leaf,
        n_jobs=args.n_jobs
    )
    
    # Save
    save_model(forest, args.output, metrics)
    
    print("\n" + "=" * 50)
    print("Training complete!")
    print(f"Run 'python export_model.py --model {args.output}/forest_model.joblib' to export for C++")
    print("=" * 50)


if __name__ == '__main__':
    main()
