#!/usr/bin/env python3
"""
Ouster LiDAR Setup and Visualization Script

This script:
1. Configures the Mac network interface for Ouster communication
2. Configures the Ouster sensor settings (UDP destination, port)
3. Verifies data flow
4. Captures a frame and renders it in 3D

Requirements:
    pip3 install numpy matplotlib requests

Usage:
    python3 setup_ouster.py
"""

import subprocess
import socket
import struct
import time
import sys
import json
import math
from typing import List, Tuple, Optional
import numpy as np

# Try to import visualization libraries
try:
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d import Axes3D
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Note: matplotlib not available. Install with: pip3 install matplotlib")

try:
    import requests
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False
    print("Note: requests not available. Install with: pip3 install requests")

# ============================================================================
# Configuration
# ============================================================================
OUSTER_IP = "172.17.1.56"
MAC_IP = "172.17.1.100"
LIDAR_PORT = 7504  # Using 7504 to avoid zombie socket on 7502
ETHERNET_IF = "en8"  # USB Ethernet adapter

# OS0-128 beam configuration (approximate angles)
NUM_BEAMS = 128
COLUMNS_PER_FRAME = 1024
ENCODER_TICKS_PER_REV = 90112


# ============================================================================
# Ouster Packet Parsing (Legacy profile for v2.3.x firmware)
# ============================================================================

class LidarColumnHeader:
    """Header for each column in a lidar packet"""
    SIZE = 16  # bytes
    
    def __init__(self, data: bytes):
        self.timestamp = struct.unpack('<Q', data[0:8])[0]
        self.measurement_id = struct.unpack('<H', data[8:10])[0]
        self.frame_id = struct.unpack('<H', data[10:12])[0]
        self.encoder_count = struct.unpack('<I', data[12:16])[0]


class LidarPixel:
    """Single pixel measurement"""
    SIZE = 12  # bytes for legacy profile
    
    def __init__(self, data: bytes):
        self.range_mm = struct.unpack('<I', data[0:4])[0]
        self.signal = struct.unpack('<H', data[4:6])[0]
        self.reflectivity = struct.unpack('<H', data[6:8])[0]
        self.near_ir = struct.unpack('<H', data[8:10])[0]


def get_beam_angles() -> Tuple[List[float], List[float]]:
    """Get beam altitude and azimuth angles for OS0-128"""
    # These are approximate values - ideally load from sensor metadata
    altitude_angles = []
    azimuth_angles = []
    
    for i in range(NUM_BEAMS):
        # OS0-128 has roughly +/- 45 degree vertical FOV
        alt = math.radians(45.0 - 90.0 * i / (NUM_BEAMS - 1))
        altitude_angles.append(alt)
        azimuth_angles.append(0.0)  # Per-beam azimuth offset (usually small)
    
    return altitude_angles, azimuth_angles


def parse_lidar_packet(data: bytes, beam_altitudes: List[float], beam_azimuths: List[float]) -> List[Tuple[float, float, float, float]]:
    """
    Parse an Ouster lidar packet and extract 3D points.
    
    Returns list of (x, y, z, intensity) tuples in meters.
    """
    points = []
    
    column_header_size = LidarColumnHeader.SIZE
    pixel_size = LidarPixel.SIZE
    column_size = column_header_size + NUM_BEAMS * pixel_size
    
    offset = 0
    while offset + column_size <= len(data):
        # Parse column header
        header = LidarColumnHeader(data[offset:offset + column_header_size])
        
        # Calculate azimuth angle for this column
        azimuth = 2.0 * math.pi * header.encoder_count / ENCODER_TICKS_PER_REV
        
        # Parse each pixel in the column
        pixel_offset = offset + column_header_size
        for beam in range(NUM_BEAMS):
            pixel_data = data[pixel_offset:pixel_offset + pixel_size]
            if len(pixel_data) < pixel_size:
                break
                
            pixel = LidarPixel(pixel_data)
            
            # Convert range from mm to meters
            range_m = pixel.range_mm * 0.001
            
            # Skip invalid points (range 0 or too far)
            if range_m < 0.3 or range_m > 50.0:
                pixel_offset += pixel_size
                continue
            
            # Calculate 3D position
            altitude = beam_altitudes[beam]
            az = azimuth + beam_azimuths[beam]
            
            x = range_m * math.cos(altitude) * math.cos(az)
            y = range_m * math.cos(altitude) * math.sin(az)
            z = range_m * math.sin(altitude)
            
            # Normalize intensity (signal typically 0-65535)
            intensity = pixel.signal / 65535.0
            
            points.append((x, y, z, intensity))
            pixel_offset += pixel_size
        
        offset += column_size
    
    return points


# ============================================================================
# Network Setup
# ============================================================================

def run_command(cmd: str, sudo: bool = False) -> Tuple[int, str, str]:
    """Run a shell command and return (returncode, stdout, stderr)"""
    if sudo:
        cmd = f"sudo {cmd}"
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    return result.returncode, result.stdout, result.stderr


def check_ethernet_interface() -> Optional[str]:
    """Find and verify the Ethernet interface"""
    print("[1/5] Checking Ethernet adapter...")
    
    # Check primary interface first
    for iface in [ETHERNET_IF, "en7", "en9", "en10", "en11", "en12"]:
        ret, out, _ = run_command(f"ifconfig {iface} 2>/dev/null")
        if ret == 0 and "status: active" in out:
            print(f"  ✓ Using interface: {iface}")
            return iface
    
    # Check if en8 exists even if not "active" (might just be connected)
    ret, out, _ = run_command(f"ifconfig {ETHERNET_IF} 2>/dev/null")
    if ret == 0:
        print(f"  ✓ Using interface: {ETHERNET_IF}")
        return ETHERNET_IF
    
    print("  ✗ No Ethernet adapter found!")
    print("  Make sure the USB-C Ethernet adapter is connected.")
    return None


def configure_ip_address(interface: str) -> bool:
    """Configure IP alias on the Ethernet interface"""
    print("\n[2/5] Configuring IP address...")
    
    # Always try to add the IP (even if it exists, this ensures it's correct)
    # First check if it's there
    ret, out, _ = run_command(f"ifconfig {interface}")
    
    if MAC_IP in out:
        print(f"  ✓ IP {MAC_IP} already configured")
    else:
        print(f"  Adding IP alias {MAC_IP}...")
        # Add the IP alias
        ret, _, err = run_command(f"ifconfig {interface} inet {MAC_IP} netmask 255.255.255.0 alias", sudo=True)
        if ret != 0:
            print(f"  ✗ Failed to add IP: {err}")
            print("\n  You may need to run this script with sudo, or run:")
            print(f"    sudo ifconfig {interface} inet {MAC_IP} netmask 255.255.255.0 alias")
            return False
        print(f"  ✓ Added {MAC_IP} to {interface}")
    
    # Verify the IP is actually there now
    ret, out, _ = run_command(f"ifconfig {interface}")
    if MAC_IP not in out:
        print(f"  ✗ IP {MAC_IP} not found after configuration!")
        print(f"  Current config:\n{out}")
        return False
    
    return True


def test_connectivity(retries: int = 3) -> bool:
    """Test if the Ouster sensor is reachable"""
    print("\n[3/5] Testing connectivity to sensor...")
    
    for attempt in range(retries):
        ret, _, _ = run_command(f"ping -c 1 -W 2 {OUSTER_IP}")
        if ret == 0:
            print(f"  ✓ Ouster sensor reachable at {OUSTER_IP}")
            return True
        if attempt < retries - 1:
            print(f"  Attempt {attempt + 1}/{retries} failed, retrying...")
            time.sleep(1)
    
    print(f"  ✗ Cannot reach sensor at {OUSTER_IP}")
    print("\nTroubleshooting:")
    print("  - Check that the Ouster is powered (solid blue LED)")
    print("  - Verify Ethernet cable is connected")
    print("  - Try running: sudo ifconfig en8 inet 172.17.1.100 netmask 255.255.255.0 alias")
    return False


def configure_sensor() -> Tuple[str, int]:
    """Configure the Ouster sensor UDP settings"""
    print("\n[4/5] Configuring sensor settings...")
    
    if not HAS_REQUESTS:
        print("  ⚠ requests library not available, skipping sensor config")
        return MAC_IP, LIDAR_PORT
    
    try:
        # Set UDP destination to auto (uses the IP that made the request)
        print(f"  Setting UDP destination to {MAC_IP}...")
        requests.post(f"http://{OUSTER_IP}/api/v1/sensor/cmd/set_udp_dest_auto", timeout=5)
    except Exception as e:
        print(f"  ⚠ Could not set UDP dest: {e}")
    
    try:
        # Try to set lidar port using ouster-cli if available
        print(f"  Setting lidar port to {LIDAR_PORT}...")
        ouster_cli = None
        for path in ["/Users/rgrigorii/Library/Python/3.9/bin/ouster-cli", "ouster-cli"]:
            ret, _, _ = run_command(f"which {path} 2>/dev/null || test -f {path}")
            if ret == 0:
                ouster_cli = path
                break
        
        if ouster_cli:
            run_command(f"{ouster_cli} source {OUSTER_IP} config udp_port_lidar {LIDAR_PORT} 2>/dev/null")
    except Exception as e:
        print(f"  ⚠ Could not set lidar port: {e}")
    
    try:
        # Reinitialize sensor
        print("  Reinitializing sensor (this takes ~8 seconds)...")
        requests.post(f"http://{OUSTER_IP}/api/v1/sensor/cmd/reinitialize", timeout=5)
        # Wait for sensor to fully reinitialize and start sending data
        for i in range(8):
            time.sleep(1)
            print(f"    Waiting... {8-i}s")
    except Exception as e:
        print(f"  ⚠ Could not reinitialize: {e}")
    
    # Verify configuration
    try:
        print("  Verifying configuration...")
        resp = requests.get(f"http://{OUSTER_IP}/api/v1/sensor/metadata", timeout=5)
        metadata = resp.json()
        config = metadata.get("config_params", {})
        
        udp_dest = config.get("udp_dest", "unknown")
        lidar_port = config.get("udp_port_lidar", LIDAR_PORT)
        
        print(f"  ✓ UDP destination: {udp_dest}")
        print(f"  ✓ Lidar port: {lidar_port}")
        
        return udp_dest, lidar_port
    except Exception as e:
        print(f"  ⚠ Could not verify config: {e}")
        return MAC_IP, LIDAR_PORT


def test_data_flow(port: int, timeout: float = 5.0, retries: int = 3) -> Optional[bytes]:
    """Test if lidar data is flowing and return a packet"""
    print("\n[5/5] Testing data flow...")
    
    for attempt in range(retries):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.settimeout(timeout)
        
        try:
            sock.bind(('', port))
            data, addr = sock.recvfrom(65536)
            print(f"  ✓ Receiving data from {addr[0]} ({len(data)} bytes)")
            return data
        except socket.timeout:
            if attempt < retries - 1:
                print(f"  Attempt {attempt + 1}/{retries}: No data yet, waiting...")
                time.sleep(2)
            else:
                print("  ✗ No data received (timeout after all retries)")
            sock.close()
        except OSError as e:
            print(f"  ✗ Port {port} error: {e}")
            sock.close()
            return None
    
    return None


def capture_frame(port: int, timeout: float = 10.0) -> List[Tuple[float, float, float, float]]:
    """Capture a complete frame of lidar data"""
    print("\n[6/6] Capturing lidar frame...")
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4 * 1024 * 1024)  # 4MB buffer
    sock.settimeout(timeout)
    
    beam_altitudes, beam_azimuths = get_beam_angles()
    
    all_points = []
    current_frame_id = None
    packets_received = 0
    
    try:
        sock.bind(('', port))
        start_time = time.time()
        
        while time.time() - start_time < timeout:
            try:
                data, addr = sock.recvfrom(65536)
                packets_received += 1
                
                # Parse the first column header to get frame ID
                if len(data) >= LidarColumnHeader.SIZE:
                    header = LidarColumnHeader(data[:LidarColumnHeader.SIZE])
                    
                    if current_frame_id is None:
                        current_frame_id = header.frame_id
                        print(f"  Starting capture of frame {current_frame_id}...")
                    elif header.frame_id != current_frame_id:
                        # New frame started, we have a complete frame
                        print(f"  ✓ Captured frame {current_frame_id} with {len(all_points)} points")
                        break
                    
                    # Parse points from this packet
                    points = parse_lidar_packet(data, beam_altitudes, beam_azimuths)
                    all_points.extend(points)
                    
            except socket.timeout:
                continue
        
        print(f"  Received {packets_received} packets")
        return all_points
        
    except OSError as e:
        print(f"  ✗ Socket error: {e}")
        return []
    finally:
        sock.close()


def render_point_cloud(points: List[Tuple[float, float, float, float]], max_points: int = 50000):
    """Render the point cloud in 3D using matplotlib"""
    if not HAS_MATPLOTLIB:
        print("\n✗ Cannot render: matplotlib not installed")
        print("  Install with: pip3 install matplotlib")
        return
    
    if not points:
        print("\n✗ No points to render")
        return
    
    print(f"\nRendering {len(points)} points (showing up to {max_points})...")
    
    # Convert to numpy arrays
    points_array = np.array(points)
    
    # Subsample if too many points
    if len(points_array) > max_points:
        indices = np.random.choice(len(points_array), max_points, replace=False)
        points_array = points_array[indices]
    
    x = points_array[:, 0]
    y = points_array[:, 1]
    z = points_array[:, 2]
    intensity = points_array[:, 3]
    
    # Create figure
    fig = plt.figure(figsize=(14, 10))
    
    # 3D scatter plot
    ax = fig.add_subplot(111, projection='3d')
    
    # Color by intensity (or height)
    colors = plt.cm.viridis(intensity)
    
    scatter = ax.scatter(x, y, z, c=intensity, cmap='viridis', s=0.5, alpha=0.6)
    
    ax.set_xlabel('X (m)')
    ax.set_ylabel('Y (m)')
    ax.set_zlabel('Z (m)')
    ax.set_title(f'Ouster OS0-128 Point Cloud\n{len(points_array)} points')
    
    # Equal aspect ratio
    max_range = np.array([x.max() - x.min(), y.max() - y.min(), z.max() - z.min()]).max() / 2.0
    mid_x = (x.max() + x.min()) * 0.5
    mid_y = (y.max() + y.min()) * 0.5
    mid_z = (z.max() + z.min()) * 0.5
    ax.set_xlim(mid_x - max_range, mid_x + max_range)
    ax.set_ylim(mid_y - max_range, mid_y + max_range)
    ax.set_zlim(mid_z - max_range, mid_z + max_range)
    
    plt.colorbar(scatter, label='Intensity', shrink=0.5)
    
    # Add statistics
    stats_text = f"""
Points: {len(points_array):,}
X range: [{x.min():.2f}, {x.max():.2f}] m
Y range: [{y.min():.2f}, {y.max():.2f}] m  
Z range: [{z.min():.2f}, {z.max():.2f}] m
"""
    fig.text(0.02, 0.02, stats_text, fontsize=9, family='monospace',
             bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    plt.tight_layout()
    
    # Save to file
    output_file = "/Users/rgrigorii/Desktop/projects/thegame/camera_system/ouster/ouster_pointcloud.png"
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"  ✓ Saved to {output_file}")
    
    # Show interactive plot
    print("  Displaying interactive 3D view (close window to exit)...")
    plt.show()


# ============================================================================
# Main
# ============================================================================

def main():
    print("=" * 50)
    print("   Ouster LiDAR Setup & Visualization")
    print("=" * 50)
    print()
    
    # Step 1: Check Ethernet interface
    interface = check_ethernet_interface()
    if not interface:
        sys.exit(1)
    
    # Step 2: Configure IP address
    if not configure_ip_address(interface):
        sys.exit(1)
    
    # Step 3: Test connectivity
    if not test_connectivity():
        sys.exit(1)
    
    # Step 4: Configure sensor
    udp_dest, lidar_port = configure_sensor()
    
    # Step 5: Test data flow
    test_packet = test_data_flow(lidar_port)
    if not test_packet:
        print("\n" + "=" * 50)
        print("   Setup Incomplete - Data Not Flowing")
        print("=" * 50)
        print("\nPossible issues:")
        print(f"  1. Firewall blocking UDP port {lidar_port}")
        print("  2. Another application using the port")
        print("  3. Sensor needs power cycle")
        print(f"\nTry: sudo lsof -i UDP:{lidar_port}")
        sys.exit(1)
    
    print("\n" + "=" * 50)
    print("   Setup Complete!")
    print("=" * 50)
    print()
    print(f"Sensor: OS-0-128 (SN: 122212001833)")
    print(f"IP: {OUSTER_IP}")
    print(f"UDP Port: {lidar_port}")
    print(f"UDP Dest: {udp_dest}")
    print()
    
    # Step 6: Capture and render frame
    points = capture_frame(lidar_port)
    
    if points:
        render_point_cloud(points)
    else:
        print("\n✗ Failed to capture frame")
        sys.exit(1)
    
    print("\nDone!")


if __name__ == "__main__":
    main()
