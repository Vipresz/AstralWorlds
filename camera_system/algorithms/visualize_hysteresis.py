#!/usr/bin/env python3
"""
Simple hysteresis curve visualization.

Strategy 1: Original eviction policy
- Increment: based on current value
- Decrement: based on (1 - current value)

Strategy 2: New eviction policy with removal count
- Track removed count for each grid cell
- If occupied: reset count to 0, multiply weight by increment
- If unoccupied: increment count, subtract (decay^count)/100 from weight
- If count reaches 5: remove from map
"""

import matplotlib.pyplot as plt
import numpy as np

# Strategy 1 Parameters
INCREMENT_FACTOR = 2  # How much to increment based on current value
DECREMENT_FACTOR = 2 # How much to decrement based on (1 - current value)
MIN_START_VALUE = 0.1

# Strategy 2 Parameters
INCREMENT_VALUE = 2  # Multiply weight by this when occupied
DECAY_VALUE = 2.5  # Decay value for unoccupied cells
REMOVAL_THRESHOLD = 100  # Remove cell when count reaches this
MIN_START_VALUE_STRATEGY2 = 0.1  # Minimum weight value to start from

def increment_delta(current_value):
    """Increment based on current value"""
    return (MIN_START_VALUE + current_value * INCREMENT_FACTOR)**2

def decrement_delta(current_value):
    """Decrement based on (1 - current value)"""
    return (MIN_START_VALUE + (1.0 - current_value) * DECREMENT_FACTOR)**2

def plot_hysteresis_loop(num_increment_steps=10, num_decrement_steps=10):
    """Plot hysteresis loop: forward N steps, back N steps"""
    # Start from low value
    value = 0.01
    
    # Forward phase (increment)
    forward_values = []
    forward_deltas = []
    
    for step in range(num_increment_steps):
        forward_values.append(value)
        delta = increment_delta(value)
        forward_deltas.append(delta)
        value = min(1.0, value + delta)
    
    # Backward phase (decrement)
    backward_values = []
    backward_deltas = []
    
    for step in range(num_decrement_steps):
        backward_values.append(value)
        delta = decrement_delta(value)
        backward_deltas.append(-delta)  # Negative for visualization
        value = max(0.0, value - delta)
    
    # Create figure with two subplots
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))
    fig.suptitle(f'Hysteresis Loop: {num_increment_steps} Steps Forward → {num_decrement_steps} Steps Back', 
                 fontsize=14, fontweight='bold')
    
    # Plot 1: Value vs Step
    ax1 = axes[0]
    forward_steps = list(range(num_increment_steps))
    backward_steps = list(range(num_increment_steps, num_increment_steps + num_decrement_steps))
    
    ax1.plot(forward_steps, forward_values, 'b-', linewidth=2, label='Forward (Increment)', 
             marker='o', markersize=6, alpha=0.7)
    ax1.plot(backward_steps, backward_values, 'r-', linewidth=2, label='Backward (Decrement)', 
             marker='s', markersize=6, alpha=0.7)
    ax1.axvline(x=num_increment_steps, color='k', linestyle='--', linewidth=1, alpha=0.5, label='Switch Point')
    ax1.set_xlabel('Step Number', fontsize=11)
    ax1.set_ylabel('Value', fontsize=11)
    ax1.set_title('Value Evolution: Forward → Backward', fontsize=12, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    ax1.legend()
    ax1.set_xlim(-0.5, num_increment_steps + num_decrement_steps - 0.5)
    max_val = max(max(forward_values) if forward_values else 0, max(backward_values) if backward_values else 0)
    ax1.set_ylim(0, max_val * 1.1 if max_val > 0 else 1.1)
    
    # Plot 2: Hysteresis area (Value vs Step, with backward indices decremented)
    ax2 = axes[1]
    
    # Forward path: step numbers 0 to num_increment_steps-1
    forward_steps_plot = list(range(num_increment_steps))
    
    # Backward path: step numbers decremented (start from num_increment_steps-1 and go down)
    # This creates the hysteresis area visualization
    backward_steps_plot = [num_increment_steps - 1 - i for i in range(num_decrement_steps)]
    
    ax2.plot(forward_steps_plot, forward_values, 'b-', linewidth=2, label='Forward Path', 
             marker='o', markersize=6, alpha=0.7)
    ax2.plot(backward_steps_plot, backward_values, 'r-', linewidth=2, label='Backward Path', 
             marker='s', markersize=6, alpha=0.7)
    
    # Fill the hysteresis area between the two paths
    # Create combined x and y arrays for filling
    if forward_values and backward_values:
        # Forward path x coordinates
        fill_x_forward = forward_steps_plot
        fill_y_forward = forward_values
        
        # Backward path x coordinates (reversed to match forward direction)
        fill_x_backward = backward_steps_plot[::-1]  # Reverse to go from low to high step
        fill_y_backward = backward_values[::-1]  # Reverse to match
        
        # Combine for closed polygon
        fill_x = fill_x_forward + fill_x_backward
        fill_y = fill_y_forward + fill_y_backward
        
        ax2.fill(fill_x, fill_y, alpha=0.2, color='purple', label='Hysteresis Area')
    
    # Add arrows to show direction
    if len(forward_values) > 1:
        for i in range(len(forward_values) - 1):
            ax2.annotate('', xy=(forward_steps_plot[i+1], forward_values[i+1]), 
                        xytext=(forward_steps_plot[i], forward_values[i]),
                        arrowprops=dict(arrowstyle='->', color='blue', lw=1.5, alpha=0.5))
    
    if len(backward_values) > 1:
        for i in range(len(backward_values) - 1):
            ax2.annotate('', xy=(backward_steps_plot[i+1], backward_values[i+1]), 
                        xytext=(backward_steps_plot[i], backward_values[i]),
                        arrowprops=dict(arrowstyle='->', color='red', lw=1.5, alpha=0.5))
    
    # Mark start and end points
    if forward_values:
        ax2.plot(forward_steps_plot[0], forward_values[0], 'go', markersize=10, 
                label='Start', zorder=5)
    if backward_values:
        ax2.plot(backward_steps_plot[-1], backward_values[-1], 'ro', markersize=10, 
                label='End', zorder=5)
    
    ax2.set_xlabel('Step Number', fontsize=11)
    ax2.set_ylabel('Value', fontsize=11)
    ax2.set_title('Hysteresis Area: Value vs Step (Backward Decremented)', fontsize=12, fontweight='bold')
    ax2.grid(True, alpha=0.3)
    ax2.legend()
    ax2.set_xlim(-0.5, num_increment_steps - 0.5)
    max_val = max(max(forward_values) if forward_values else 0, max(backward_values) if backward_values else 0)
    ax2.set_ylim(0, max_val * 1.1 if max_val > 0 else 1.1)
    
    plt.tight_layout()
    return fig

def plot_strategy2_hysteresis(num_occupied_steps=10, num_unoccupied_steps=10):
    """Plot Strategy 2: Eviction with removal count"""
    # Simulate a grid cell
    weight = MIN_START_VALUE_STRATEGY2  # Start with minimum value
    removed_count = 0
    is_occupied = True
    
    # Track values over time
    weights = []
    counts = []
    occupied_states = []
    steps = []
    
    # Forward phase: occupied
    for step in range(num_occupied_steps):
        if is_occupied:
            removed_count = 0  # Reset count when occupied
            # Multiply by increment, but ensure we don't go below min start value
            weight = min(1.0, max(MIN_START_VALUE_STRATEGY2, weight * INCREMENT_VALUE))
        weights.append(weight)
        counts.append(removed_count)
        occupied_states.append(1 if is_occupied else 0)
        steps.append(step)
    
    # Backward phase: unoccupied
    is_occupied = False
    for step in range(num_occupied_steps, num_occupied_steps + num_unoccupied_steps):
        if not is_occupied:
            removed_count += 1
            if removed_count >= REMOVAL_THRESHOLD:
                # Cell is removed from map
                weight = 0.0
                weights.append(weight)
                counts.append(removed_count)
                occupied_states.append(0)
                steps.append(step)
                break
            else:
                # Subtract (decay^count)/100
                decay_amount = (DECAY_VALUE ** removed_count) / 100.0
                weight = max(0.0, weight - decay_amount)
                # Ensure weight doesn't go below minimum start value (unless being removed)
                if weight > 0.0:
                    weight = max(MIN_START_VALUE_STRATEGY2, weight)
        weights.append(weight)
        counts.append(removed_count)
        occupied_states.append(0)
        steps.append(step)
    
    # Create figure with two subplots
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))
    fig.suptitle(f'Strategy 2: Eviction with Removal Count ({num_occupied_steps} Occupied → {num_unoccupied_steps} Unoccupied)', 
                 fontsize=14, fontweight='bold')
    
    # Plot 1: Weight vs Step
    ax1 = axes[0]
    occupied_steps = [s for s, o in zip(steps, occupied_states) if o == 1]
    unoccupied_steps = [s for s, o in zip(steps, occupied_states) if o == 0]
    occupied_weights = [w for w, o in zip(weights, occupied_states) if o == 1]
    unoccupied_weights = [w for w, o in zip(weights, occupied_states) if o == 0]
    
    ax1.plot(occupied_steps, occupied_weights, 'g-', linewidth=2, label='Occupied (Increment)', 
             marker='o', markersize=6, alpha=0.7)
    ax1.plot(unoccupied_steps, unoccupied_weights, 'r-', linewidth=2, label='Unoccupied (Decay)', 
             marker='s', markersize=6, alpha=0.7)
    if num_occupied_steps < len(steps):
        ax1.axvline(x=num_occupied_steps, color='k', linestyle='--', linewidth=1, alpha=0.5, label='Switch Point')
    ax1.set_xlabel('Step Number', fontsize=11)
    ax1.set_ylabel('Weight', fontsize=11)
    ax1.set_title('Weight Evolution: Occupied → Unoccupied', fontsize=12, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    ax1.legend()
    ax1.set_xlim(-0.5, max(steps) + 0.5 if steps else 10.5)
    max_weight = max(weights) if weights else 1.0
    ax1.set_ylim(0, max_weight * 1.1 if max_weight > 0 else 1.1)
    
    # Plot 2: Removal Count vs Step
    ax2 = axes[1]
    ax2.plot(steps, counts, 'b-', linewidth=2, label='Removal Count', 
             marker='o', markersize=6, alpha=0.7)
    ax2.axhline(y=REMOVAL_THRESHOLD, color='r', linestyle='--', linewidth=2, 
                label=f'Removal Threshold ({REMOVAL_THRESHOLD})', alpha=0.7)
    if num_occupied_steps < len(steps):
        ax2.axvline(x=num_occupied_steps, color='k', linestyle='--', linewidth=1, alpha=0.5, label='Switch Point')
    ax2.set_xlabel('Step Number', fontsize=11)
    ax2.set_ylabel('Removal Count', fontsize=11)
    ax2.set_title('Removal Count: Resets on Occupied, Increments on Unoccupied', fontsize=12, fontweight='bold')
    ax2.grid(True, alpha=0.3)
    ax2.legend()
    ax2.set_xlim(-0.5, max(steps) + 0.5 if steps else 10.5)
    max_count = max(counts) if counts else REMOVAL_THRESHOLD
    ax2.set_ylim(-0.5, max(max_count, REMOVAL_THRESHOLD) + 0.5)
    
    plt.tight_layout()
    return fig

def main():
    """Main function"""
    print("Generating hysteresis curves...")
    print(f"\nStrategy 1: Original Eviction Policy")
    print(f"Increment factor: {INCREMENT_FACTOR} (based on current value)")
    print(f"Decrement factor: {DECREMENT_FACTOR} (based on 1 - current value)")
    print()
    
    # Generate Strategy 1 plot
    fig1 = plot_hysteresis_loop(num_increment_steps=10, num_decrement_steps=10)
    fig1.savefig('hysteresis_loop_strategy1.png', dpi=150, bbox_inches='tight')
    print("Saved: hysteresis_loop_strategy1.png")
    
    # Print Strategy 1 values
    value = 0.01
    print("\nStrategy 1 - Forward phase (10 steps):")
    for step in range(10):
        delta = increment_delta(value)
        print(f"  Step {step}: value = {value:.6f}, delta = {delta:.6f}")
        value = min(1.0, value + delta)
    
    print("\nStrategy 1 - Backward phase (10 steps):")
    for step in range(10):
        delta = decrement_delta(value)
        print(f"  Step {step}: value = {value:.6f}, delta = {decrement_delta(value):.6f}")
        value = max(0.0, value - delta)
    
    print("\n" + "="*60)
    print(f"\nStrategy 2: New Eviction Policy with Removal Count")
    print(f"Increment value: {INCREMENT_VALUE} (multiply weight when occupied)")
    print(f"Decay value: {DECAY_VALUE} (used in (decay^count)/100 formula)")
    print(f"Removal threshold: {REMOVAL_THRESHOLD} (remove when count reaches this)")
    print(f"Min start value: {MIN_START_VALUE_STRATEGY2} (minimum weight to start from)")
    print()
    
    # Generate Strategy 2 plot
    fig2 = plot_strategy2_hysteresis(num_occupied_steps=10, num_unoccupied_steps=10)
    fig2.savefig('hysteresis_loop_strategy2.png', dpi=150, bbox_inches='tight')
    print("Saved: hysteresis_loop_strategy2.png")
    
    # Print Strategy 2 values
    weight = MIN_START_VALUE_STRATEGY2
    removed_count = 0
    is_occupied = True
    print("\nStrategy 2 - Occupied phase (10 steps):")
    for step in range(10):
        if is_occupied:
            removed_count = 0
            old_weight = weight
            weight = min(1.0, max(MIN_START_VALUE_STRATEGY2, weight * INCREMENT_VALUE))
            print(f"  Step {step}: weight = {old_weight:.6f} → {weight:.6f} (×{INCREMENT_VALUE}), count = {removed_count}")
    
    is_occupied = False
    print("\nStrategy 2 - Unoccupied phase (until removal or 10 steps):")
    for step in range(10, 20):
        if not is_occupied:
            removed_count += 1
            if removed_count >= REMOVAL_THRESHOLD:
                print(f"  Step {step}: REMOVED (count reached {REMOVAL_THRESHOLD})")
                break
            else:
                decay_amount = (DECAY_VALUE ** removed_count) / 100.0
                old_weight = weight
                weight = max(0.0, weight - decay_amount)
                # Ensure weight doesn't go below minimum start value (unless being removed)
                if weight > 0.0:
                    weight = max(MIN_START_VALUE_STRATEGY2, weight)
                print(f"  Step {step}: weight = {old_weight:.6f} → {weight:.6f} (-{decay_amount:.6f}), count = {removed_count}")
    
    print("\nObservations:")
    print("Strategy 1:")
    print("- Forward: Increment is small at low values, larger at high values")
    print("- Backward: Decrement is large at high values, smaller at low values")
    print("- This creates hysteresis: the path forward ≠ path backward")
    print("\nStrategy 2:")
    print("- Occupied: Count resets to 0, weight multiplies by increment")
    print("- Unoccupied: Count increments, weight decays by (decay^count)/100")
    print("- Removal: Cell removed from map when count reaches threshold")
    
    # Create combined figure with both strategies
    fig_combined, axes_combined = plt.subplots(2, 2, figsize=(16, 10))
    fig_combined.suptitle('Eviction Policy Comparison: Strategy 1 vs Strategy 2', 
                          fontsize=16, fontweight='bold')
    
    # Strategy 1 - Recreate plots
    value = 0.01
    forward_values = []
    backward_values = []
    for step in range(10):
        forward_values.append(value)
        delta = increment_delta(value)
        value = min(1.0, value + delta)
    for step in range(10):
        backward_values.append(value)
        delta = decrement_delta(value)
        value = max(0.0, value - delta)
    
    forward_steps = list(range(10))
    backward_steps = list(range(10, 20))
    forward_steps_plot = list(range(10))
    backward_steps_plot = [9 - i for i in range(10)]
    
    # Strategy 1 - Plot 1: Value vs Step
    axes_combined[0, 0].plot(forward_steps, forward_values, 'b-', linewidth=2, label='Forward (Increment)', 
                             marker='o', markersize=6, alpha=0.7)
    axes_combined[0, 0].plot(backward_steps, backward_values, 'r-', linewidth=2, label='Backward (Decrement)', 
                             marker='s', markersize=6, alpha=0.7)
    axes_combined[0, 0].axvline(x=10, color='k', linestyle='--', linewidth=1, alpha=0.5, label='Switch Point')
    axes_combined[0, 0].set_xlabel('Step Number', fontsize=11)
    axes_combined[0, 0].set_ylabel('Value', fontsize=11)
    axes_combined[0, 0].set_title('Strategy 1: Value Evolution', fontsize=12, fontweight='bold')
    axes_combined[0, 0].grid(True, alpha=0.3)
    axes_combined[0, 0].legend()
    
    # Strategy 1 - Plot 2: Hysteresis area
    axes_combined[0, 1].plot(forward_steps_plot, forward_values, 'b-', linewidth=2, label='Forward Path', 
                             marker='o', markersize=6, alpha=0.7)
    axes_combined[0, 1].plot(backward_steps_plot, backward_values, 'r-', linewidth=2, label='Backward Path', 
                             marker='s', markersize=6, alpha=0.7)
    if forward_values and backward_values:
        fill_x = forward_steps_plot + backward_steps_plot[::-1]
        fill_y = forward_values + backward_values[::-1]
        axes_combined[0, 1].fill(fill_x, fill_y, alpha=0.2, color='purple', label='Hysteresis Area')
    axes_combined[0, 1].set_xlabel('Step Number', fontsize=11)
    axes_combined[0, 1].set_ylabel('Value', fontsize=11)
    axes_combined[0, 1].set_title('Strategy 1: Hysteresis Area', fontsize=12, fontweight='bold')
    axes_combined[0, 1].grid(True, alpha=0.3)
    axes_combined[0, 1].legend()
    
    # Strategy 2 - Recreate plots from data
    weight = MIN_START_VALUE_STRATEGY2
    removed_count = 0
    is_occupied = True
    weights = []
    counts = []
    occupied_states = []
    steps = []
    
    for step in range(10):
        if is_occupied:
            removed_count = 0
            weight = min(1.0, max(MIN_START_VALUE_STRATEGY2, weight * INCREMENT_VALUE))
        weights.append(weight)
        counts.append(removed_count)
        occupied_states.append(1)
        steps.append(step)
    
    is_occupied = False
    for step in range(10, 20):
        if not is_occupied:
            removed_count += 1
            if removed_count >= REMOVAL_THRESHOLD:
                weight = 0.0
                weights.append(weight)
                counts.append(removed_count)
                occupied_states.append(0)
                steps.append(step)
                break
            else:
                decay_amount = (DECAY_VALUE ** removed_count) / 100.0
                weight = max(0.0, weight - decay_amount)
                # Ensure weight doesn't go below minimum start value (unless being removed)
                if weight > 0.0:
                    weight = max(MIN_START_VALUE_STRATEGY2, weight)
        weights.append(weight)
        counts.append(removed_count)
        occupied_states.append(0)
        steps.append(step)
    
    occupied_steps = [s for s, o in zip(steps, occupied_states) if o == 1]
    unoccupied_steps = [s for s, o in zip(steps, occupied_states) if o == 0]
    occupied_weights = [w for w, o in zip(weights, occupied_states) if o == 1]
    unoccupied_weights = [w for w, o in zip(weights, occupied_states) if o == 0]
    
    # Strategy 2 - Plot 1: Weight vs Step
    axes_combined[1, 0].plot(occupied_steps, occupied_weights, 'g-', linewidth=2, label='Occupied (Increment)', 
                             marker='o', markersize=6, alpha=0.7)
    axes_combined[1, 0].plot(unoccupied_steps, unoccupied_weights, 'r-', linewidth=2, label='Unoccupied (Decay)', 
                              marker='s', markersize=6, alpha=0.7)
    if 10 < len(steps):
        axes_combined[1, 0].axvline(x=10, color='k', linestyle='--', linewidth=1, alpha=0.5, label='Switch Point')
    axes_combined[1, 0].set_xlabel('Step Number', fontsize=11)
    axes_combined[1, 0].set_ylabel('Weight', fontsize=11)
    axes_combined[1, 0].set_title('Strategy 2: Weight Evolution', fontsize=12, fontweight='bold')
    axes_combined[1, 0].grid(True, alpha=0.3)
    axes_combined[1, 0].legend()
    
    # Strategy 2 - Plot 2: Removal Count vs Step
    axes_combined[1, 1].plot(steps, counts, 'b-', linewidth=2, label='Removal Count', 
                            marker='o', markersize=6, alpha=0.7)
    axes_combined[1, 1].axhline(y=REMOVAL_THRESHOLD, color='r', linestyle='--', linewidth=2, 
                               label=f'Removal Threshold ({REMOVAL_THRESHOLD})', alpha=0.7)
    if 10 < len(steps):
        axes_combined[1, 1].axvline(x=10, color='k', linestyle='--', linewidth=1, alpha=0.5, label='Switch Point')
    axes_combined[1, 1].set_xlabel('Step Number', fontsize=11)
    axes_combined[1, 1].set_ylabel('Removal Count', fontsize=11)
    axes_combined[1, 1].set_title('Strategy 2: Removal Count', fontsize=12, fontweight='bold')
    axes_combined[1, 1].grid(True, alpha=0.3)
    axes_combined[1, 1].legend()
    
    plt.tight_layout()
    fig_combined.savefig('hysteresis_comparison.png', dpi=150, bbox_inches='tight')
    print("\nSaved: hysteresis_comparison.png")
    
    # Show plots
    plt.show()

if __name__ == '__main__':
    main()

