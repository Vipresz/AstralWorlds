#!/usr/bin/env python3
"""
Ouster LiDAR PCAP Capture and Visualization

Captures live lidar data to PCAP file and metadata, or loads and plots existing PCAP files.
"""

import subprocess
import socket
import time
import sys
import os
import json
import argparse
from typing import Optional

_script_dir = os.path.dirname(os.path.abspath(__file__))
_sdk_python_path = os.path.join(_script_dir, "ouster-sdk", "python", "src")
if os.path.exists(_sdk_python_path) and _sdk_python_path not in sys.path:
    sys.path.insert(0, _sdk_python_path)

HAS_OUSTER_SDK = False
SDK_IMPORT_ERROR = None
try:
    from ouster.sdk import pcap, core, sensor
    from ouster.sdk.core import LidarScan, ChanField, SensorInfo, Scans
    HAS_OUSTER_SDK = True
except ImportError as e:
    HAS_OUSTER_SDK = False
    SDK_IMPORT_ERROR = str(e)

OUSTER_CLI_AVAILABLE = False
try:
    result = subprocess.run(["which", "ouster-cli"], capture_output=True, timeout=2)
    OUSTER_CLI_AVAILABLE = (result.returncode == 0)
except:
    pass

HAS_OPEN3D = False
try:
    import open3d as o3d
    HAS_OPEN3D = True
except ImportError:
    HAS_OPEN3D = False

HAS_MATPLOTLIB = False
try:
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d import Axes3D
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False

OUSTER_IP = "172.17.1.56"
MAC_IP = "172.17.1.100"
LIDAR_PORT = 7504


def capture_pcap(hostname: str, lidar_port: int, timeout: float = 15.0) -> Optional[str]:
    """Capture packets and save as PCAP file. Returns path to saved PCAP file."""
    if HAS_OUSTER_SDK:
        from ouster.sdk import pcap, core, sensor
        from ouster.sdk.core import LidarScan, ChanField, SensorInfo, Scans
    
    print("Capturing packets and saving as PCAP...")
    
    if OUSTER_CLI_AVAILABLE:
        print("  Using ouster-cli to record PCAP...")
        script_dir = os.path.dirname(os.path.abspath(__file__))
        pcap_file = os.path.join(script_dir, "ouster.pcap")
        metadata_file = os.path.join(script_dir, "metadata.json")
        
        try:
            print(f"  Recording to {pcap_file}...")
            capture_time = 5.0
            cmd_variations = [
                ["ouster-cli", "source", hostname, "record", "-o", pcap_file],
                ["ouster-cli", "source", hostname, "record", "--output", pcap_file],
                ["ouster-cli", "source", hostname, "record", pcap_file],
            ]
            
            for i, cmd in enumerate(cmd_variations):
                if i > 0:
                    print(f"  Trying syntax variation {i+1}...")
                
                result = subprocess.run(
                    cmd,
                    timeout=capture_time,
                    capture_output=True,
                    text=True
                )
                
                if os.path.exists(pcap_file) and os.path.getsize(pcap_file) > 0:
                    print(f"  ✓ PCAP file created")
                    break
                
                if "unexpected" in result.stderr.lower() or "unexpected" in result.stdout.lower():
                    continue
                
                if result.returncode == 0 and i == len(cmd_variations) - 1:
                    with open(pcap_file, 'wb') as f:
                        redirect_cmd = ["ouster-cli", "source", hostname, "record"]
                        redirect_result = subprocess.run(
                            redirect_cmd,
                            timeout=capture_time,
                            stdout=f,
                            stderr=subprocess.PIPE,
                            text=False
                        )
                        if os.path.exists(pcap_file) and os.path.getsize(pcap_file) > 0:
                            result = redirect_result
                            break
                    break
            
            if os.path.exists(pcap_file) and os.path.getsize(pcap_file) > 0:
                file_size = os.path.getsize(pcap_file)
                print(f"  ✓ PCAP recorded: {pcap_file} ({file_size} bytes)")
                
                print("  Fetching sensor metadata...")
                try:
                    metadata_cmd = ["ouster-cli", "source", hostname, "metadata"]
                    metadata_result = subprocess.run(
                        metadata_cmd,
                        timeout=10,
                        capture_output=True,
                        text=True
                    )
                    if metadata_result.returncode == 0 and metadata_result.stdout:
                        with open(metadata_file, 'w') as f:
                            f.write(metadata_result.stdout)
                        print(f"  ✓ Metadata saved: {metadata_file}")
                    else:
                        print(f"  ⚠ Could not fetch metadata via ouster-cli")
                except Exception as e:
                    print(f"  ⚠ Could not fetch metadata: {e}")
                
                return pcap_file
            else:
                print(f"  ⚠ PCAP file not created or empty")
                print("  Falling back to SDK capture...")
        except subprocess.TimeoutExpired:
            if os.path.exists(pcap_file) and os.path.getsize(pcap_file) > 0:
                file_size = os.path.getsize(pcap_file)
                print(f"  ✓ PCAP recorded: {pcap_file} ({file_size} bytes)")
                try:
                    metadata_cmd = ["ouster-cli", "source", hostname, "metadata"]
                    metadata_result = subprocess.run(
                        metadata_cmd,
                        timeout=10,
                        capture_output=True,
                        text=True
                    )
                    if metadata_result.returncode == 0 and metadata_result.stdout:
                        with open(metadata_file, 'w') as f:
                            f.write(metadata_result.stdout)
                        print(f"  ✓ Metadata saved: {metadata_file}")
                except:
                    pass
                return pcap_file
            else:
                print("  ⚠ Timeout but no PCAP file created")
                print("  Falling back to SDK capture...")
        except Exception as e:
            print(f"  ⚠ Error using ouster-cli: {e}")
            print("  Falling back to SDK capture...")
    
    if HAS_OUSTER_SDK:
        print("  Using Ouster SDK to capture PCAP...")
        try:
            print(f"  Connecting to sensor {hostname}...")
            info = SensorInfo.from_hostname(hostname, lidar_port=lidar_port)
            print(f"  ✓ Connected: {info.prod_line} (SN: {info.sn})")
            
            script_dir = os.path.dirname(os.path.abspath(__file__))
            metadata_file = os.path.join(script_dir, "metadata.json")
            with open(metadata_file, 'w') as f:
                f.write(info.json())
            print(f"  ✓ Saved metadata to {metadata_file}")
            
            print(f"  Starting packet capture on port {lidar_port}...")
            packet_source = sensor.SensorPacketSource(hostname, lidar_port=lidar_port)
            
            packets = []
            start_time = time.time()
            packet_count = 0
            max_packets = 2000
            
            try:
                for packet in packet_source:
                    packets.append(packet)
                    packet_count += 1
                    
                    if time.time() - start_time > 10.0 or packet_count >= max_packets:
                        break
                    
                    if packet_count % 100 == 0:
                        print(f"    Captured {packet_count} packets...")
            except Exception as e:
                print(f"  ⚠ Packet capture ended: {e}")
            finally:
                packet_source.close()
            
            if len(packets) == 0:
                print("  ✗ No packets captured")
                return None
            
            pcap_file = os.path.join(script_dir, "ouster.pcap")
            print(f"  Saving {len(packets)} packets to {pcap_file}...")
            
            pcap.record(
                packets,
                pcap_file,
                src_ip=hostname,
                dst_ip=MAC_IP,
                lidar_port=lidar_port
            )
            
            print(f"  ✓ Saved PCAP file: {pcap_file}")
            return pcap_file
            
        except Exception as e:
            print(f"  ✗ Error capturing PCAP: {e}")
            import traceback
            traceback.print_exc()
            return None
    
    print("  ✗ SDK and ouster-cli not available")
    return None


def load_and_plot_pcap(pcap_file: str, metadata_file: Optional[str] = None, grid_resolution: Optional[float] = None):
    """Load a PCAP file and plot the point cloud"""
    global HAS_OUSTER_SDK, SDK_IMPORT_ERROR
    
    if HAS_OUSTER_SDK:
        from ouster.sdk import pcap, core, sensor
        from ouster.sdk.core import LidarScan, ChanField, SensorInfo, Scans
    
    print("=" * 50)
    print("   Ouster LiDAR PCAP Visualization")
    print("=" * 50)
    print()
    
    if not HAS_OUSTER_SDK:
        print("✗ Ouster SDK Python package not available - cannot parse PCAP files")
        print(f"  Import error: {SDK_IMPORT_ERROR}")
        print()
        print("  Build the SDK first:")
        print("    cd ouster-sdk/python")
        print("    pip install -e .")
        sys.exit(1)
    
    if not HAS_OPEN3D and not HAS_MATPLOTLIB:
        print("✗ No visualization library available - cannot plot point cloud")
        print("  Install Open3D (recommended): pip install open3d")
        print("  Or install matplotlib: pip install matplotlib")
        sys.exit(1)
    
    if not os.path.isabs(pcap_file):
        script_dir = os.path.dirname(os.path.abspath(__file__))
        pcap_file = os.path.join(script_dir, pcap_file)
    
    if not os.path.exists(pcap_file):
        print(f"✗ PCAP file not found: {pcap_file}")
        sys.exit(1)
    
    if metadata_file is None:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        metadata_file = os.path.join(script_dir, "metadata.json")
    
    if not os.path.exists(metadata_file):
        print(f"✗ Metadata file not found: {metadata_file}")
        sys.exit(1)
    
    print(f"Loading PCAP file: {pcap_file}")
    print(f"Using metadata: {metadata_file}")
    
    try:
        with open(metadata_file, 'r') as f:
            meta = json.load(f)
        
        range_unit = meta.get("data_format", {}).get("range_unit")
        if range_unit is None:
            range_unit = meta.get("lidar_data_format", {}).get("range_unit")
        if range_unit is None:
            print("  ⚠ range_unit not found in metadata, using default 0.001 m/count")
            range_unit = 0.001
        else:
            print(f"  ✓ Found range_unit: {range_unit} m/count")
        
        with open(metadata_file, 'r') as f:
            metadata_json = f.read()
        info = SensorInfo(metadata_json)
        print(f"  ✓ Loaded metadata: {info.prod_line} (SN: {info.sn})")
        
        src = pcap.PcapScanSource(pcap_file, sensor_info=[info])
        print(f"  ✓ Opened PCAP file")
        
        print("  Extracting point cloud from first scan...")
        scans = iter(src)
        scan_data = next(scans, None)
        
        if scan_data is None or len(scan_data) == 0:
            print("  ✗ No scans found in PCAP file")
            src.close()
            sys.exit(1)
        
        scan = scan_data[0]
        if scan is None:
            print("  ✗ Invalid scan")
            src.close()
            sys.exit(1)
        
        print(f"  ✓ Scan: {scan.w} columns x {scan.h} pixels")
        
        ranges_m = scan.field(ChanField.RANGE).astype(float) * range_unit
        
        SPEED_OF_LIGHT = 299792458.0
        tof_s = 2.0 * ranges_m / SPEED_OF_LIGHT
        print(f"  ✓ Calculated ToF: min={tof_s.min():.2e}s, max={tof_s.max():.2e}s")
        
        from ouster.sdk.core import XYZLut
        xyzlut = XYZLut(info)
        xyz = xyzlut(scan.field(ChanField.RANGE))
        
        intensity = scan.field(ChanField.SIGNAL)
        
        x = xyz[:, :, 0].flatten()
        y = xyz[:, :, 1].flatten()
        z = xyz[:, :, 2].flatten()
        i = intensity.flatten()
        
        valid = ranges_m.flatten() > 0
        x = x[valid]
        y = y[valid]
        z = z[valid]
        i = i[valid]
        
        print(f"  ✓ Extracted {len(x)} valid points")
        
        src.close()
        
        if grid_resolution is not None and grid_resolution > 0:
            print(f"  Snapping points to grid (resolution: {grid_resolution} m)...")
            x, y, z, i = snap_to_grid(x, y, z, i, grid_resolution)
            print(f"  ✓ Grid-snapped to {len(x)} points")
            title_suffix = f"(grid: {grid_resolution}m)"
        else:
            title_suffix = ""
        
        print("  Rendering point cloud...")
        render_point_cloud(x, y, z, i, len(x), title_suffix)
        
        print("\nDone!")
        
    except Exception as e:
        print(f"✗ Error loading/plotting PCAP: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


def snap_to_grid(x, y, z, intensity, grid_resolution):
    """
    Snap points to a grid by averaging points within each grid cell.
    
    Args:
        x, y, z: Point coordinates arrays
        intensity: Intensity values array
        grid_resolution: Grid cell size in meters
    
    Returns:
        Tuple of (x_grid, y_grid, z_grid, i_grid) with averaged grid points
    """
    import numpy as np
    
    if len(x) == 0:
        return x, y, z, intensity
    
    # Calculate grid cell indices for each point
    x_cell = np.floor(x / grid_resolution).astype(int)
    y_cell = np.floor(y / grid_resolution).astype(int)
    z_cell = np.floor(z / grid_resolution).astype(int)
    
    # Create a dictionary to group points by grid cell
    grid_dict = {}
    for i in range(len(x)):
        cell_key = (x_cell[i], y_cell[i], z_cell[i])
        if cell_key not in grid_dict:
            grid_dict[cell_key] = {'x': [], 'y': [], 'z': [], 'i': []}
        grid_dict[cell_key]['x'].append(x[i])
        grid_dict[cell_key]['y'].append(y[i])
        grid_dict[cell_key]['z'].append(z[i])
        grid_dict[cell_key]['i'].append(intensity[i])
    
    # Average points within each grid cell
    x_grid = []
    y_grid = []
    z_grid = []
    i_grid = []
    
    for cell_key, points in grid_dict.items():
        x_grid.append(np.mean(points['x']))
        y_grid.append(np.mean(points['y']))
        z_grid.append(np.mean(points['z']))
        i_grid.append(np.mean(points['i']))
    
    return np.array(x_grid), np.array(y_grid), np.array(z_grid), np.array(i_grid)


def render_point_cloud(x, y, z, intensity, num_points, title_suffix=""):
    """Render point cloud using Open3D (preferred) or matplotlib (fallback)"""
    import numpy as np
    
    if HAS_OPEN3D:
        render_point_cloud_open3d(x, y, z, intensity, num_points, title_suffix)
    elif HAS_MATPLOTLIB:
        render_point_cloud_matplotlib(x, y, z, intensity, num_points, title_suffix)
    else:
        print("✗ No visualization library available")
        sys.exit(1)


def render_point_cloud_open3d(x, y, z, intensity, num_points, title_suffix=""):
    """Render point cloud using Open3D - fast, interactive, best for large point clouds"""
    import numpy as np
    
    points = np.column_stack((x, y, z))
    
    pcd = o3d.geometry.PointCloud()
    pcd.points = o3d.utility.Vector3dVector(points)
    
    i_norm = (intensity - intensity.min()) / (intensity.max() - intensity.min() + 1e-6)
    
    if HAS_MATPLOTLIB:
        colors = plt.cm.viridis(i_norm)[:, :3]
    else:
        colors = viridis_colormap(i_norm)
    
    pcd.colors = o3d.utility.Vector3dVector(colors)
    
    title = f'Ouster LiDAR Point Cloud ({num_points:,} points)'
    if title_suffix:
        title += f" {title_suffix}"
    
    print(f"  Opening interactive 3D viewer...")
    print(f"  Controls: Mouse drag to rotate, Shift+drag to pan, Scroll to zoom")
    print(f"  Press 'Q' or close window to exit")
    
    o3d.visualization.draw_geometries(
        [pcd],
        window_name=title,
        width=1920,
        height=1080,
        point_show_normal=False
    )


def viridis_colormap(values):
    """Viridis-like colormap: purple -> blue -> green -> yellow"""
    import numpy as np
    
    values = np.clip(values, 0, 1)
    
    r = np.interp(values, [0, 0.25, 0.5, 0.75, 1.0], [0.27, 0.16, 0.13, 0.33, 0.99])
    g = np.interp(values, [0, 0.25, 0.5, 0.75, 1.0], [0.00, 0.11, 0.43, 0.68, 0.99])
    b = np.interp(values, [0, 0.25, 0.5, 0.75, 1.0], [0.33, 0.54, 0.71, 0.60, 0.70])
    
    return np.column_stack([r, g, b])


def render_point_cloud_matplotlib(x, y, z, intensity, num_points, title_suffix=""):
    """Render point cloud using matplotlib - fallback option"""
    import numpy as np
    
    i_norm = (intensity - intensity.min()) / (intensity.max() - intensity.min() + 1e-6)
    
    fig = plt.figure(figsize=(12, 8))
    ax = fig.add_subplot(111, projection='3d')
    
    scatter = ax.scatter(x, y, z, c=i_norm, cmap='viridis', s=0.5, alpha=0.6)
    
    ax.set_xlabel('X (m)')
    ax.set_ylabel('Y (m)')
    ax.set_zlabel('Z (m)')
    title = f'Ouster LiDAR Point Cloud ({num_points:,} points)'
    if title_suffix:
        title += f" {title_suffix}"
    ax.set_title(title)
    
    plt.colorbar(scatter, ax=ax, label='Intensity (normalized)')
    plt.tight_layout()
    plt.show()


def main():
    parser = argparse.ArgumentParser(
        description="Ouster LiDAR PCAP Capture and Visualization",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Capture live lidar data to PCAP
  python3 setup_ouster.py
  
  # Load and plot a saved PCAP file (uses Open3D if available, matplotlib otherwise)
  python3 setup_ouster.py --load-packet ouster.pcap --metadata metadata.json
  
  # Load and plot with grid snapping (10cm resolution)
  python3 setup_ouster.py --load-packet ouster.pcap --grid-resolution 0.1

Note: For best visualization, install Open3D: pip install open3d
      Open3D provides interactive 3D viewing with rotation, zoom, and pan controls.
        """
    )
    
    parser.add_argument(
        '--load-packet',
        '--load_packet',
        nargs='?',
        const='ouster.pcap',
        metavar='PCAP_FILE',
        dest='load_packet',
        help='Load and plot a saved PCAP file (default: ouster.pcap)'
    )
    
    parser.add_argument(
        '--metadata',
        metavar='METADATA_FILE',
        help='Metadata JSON file (default: metadata.json)'
    )
    
    parser.add_argument(
        '--grid-resolution',
        '--grid_resolution',
        type=float,
        metavar='RESOLUTION',
        dest='grid_resolution',
        help='Snap points to grid with given resolution in meters (e.g., 0.1 for 10cm grid)'
    )
    
    args = parser.parse_args()
    
    if args.load_packet is not None:
        pcap_file = args.load_packet if args.load_packet else 'ouster.pcap'
        load_and_plot_pcap(pcap_file, args.metadata, args.grid_resolution)
        return
    
    print("=" * 50)
    print("   Ouster LiDAR PCAP Capture")
    print("=" * 50)
    print()
    
    packet_file = capture_pcap(OUSTER_IP, LIDAR_PORT)
    if not packet_file:
        print("\n✗ Failed to capture PCAP file")
        sys.exit(1)
    
    print("\n" + "=" * 50)
    print("   Capture Complete!")
    print("=" * 50)
    print()
    print(f"PCAP file: {packet_file}")
    print(f"Metadata file: metadata.json")
    print()
    print("To visualize, run:")
    print(f"  python3 setup_ouster.py --load-packet {os.path.basename(packet_file)}")
    print()


if __name__ == "__main__":
    main()
