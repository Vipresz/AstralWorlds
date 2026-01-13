# Ouster LiDAR Integration Guide

## Your Sensor
- **Model:** OS-0-128
- **IP:** 172.17.1.56
- **Firmware:** v2.3.1
- **UDP Port:** 7502 (lidar), 7503 (IMU)

---

## ✅ Integrated into CameraSystemEditor

The Ouster LiDAR is now integrated into your Camera System Editor!

### How to Use

1. **Run the network setup script** (required before starting the editor):
   ```bash
   bash /Users/rgrigorii/Desktop/projects/ouster/setup_ouster_network.sh
   ```

2. **Start the Camera System Editor**

3. **Enable Ouster mode:**
   - Click **File** menu in the top menu bar
   - Under "TOF Data Source", check **Ouster LiDAR**
   - The editor will automatically connect and start streaming

4. **Switch back to simulated TOF:**
   - Click **File** menu
   - Check **Simulated TOF** (this automatically disables Ouster)