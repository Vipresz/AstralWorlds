# Samsung Gear 360 (2017) Streaming Integration

## Building

### Standalone Build

The `gear360` directory has its own `CMakeLists.txt` for independent building:

```bash
cd thegame/camera_system/gear360
mkdir -p build && cd build
cmake ..
make gear360_test
# or
cmake --build . --target gear360_test
```

The executable will be in `build/bin/gear360_test`.

### Building from Parent Directory

Alternatively, you can build from the parent `camera_system` directory - the main CMakeLists.txt also includes the gear360 test application.

## Configuring Camera for WiFi Streaming

1. **Power on the Gear 360 camera**
2. **Enable WiFi mode on the camera:**
   - Navigate to the camera's menu
   - Select "WiFi" or "Network" option
   - Enable WiFi AP (Access Point) mode
3. **Connect your computer to the Gear 360's WiFi network:**
   - Look for a WiFi network named something like "Gear 360" or "SM-C200"
   - The password is usually displayed on the camera's screen
   - Connect your computer to this network
4. **Note the camera's IP address:**
   - The camera typically uses `192.168.1.1` or `192.168.43.1`
   - You may need to check your network settings to confirm
   - If needed, update the IP in `Gear360Stream.cpp` in the `ReceiveVideoOverWiFi()` function

### Maintaining Internet Access While Connected

**Important:** When connected to the Gear 360's WiFi AP, you will lose your regular internet connection because your computer can only connect to one WiFi network at a time.

**Solutions to maintain internet access:**

1. **Use Ethernet connection (Recommended):**
   - Connect your computer to the internet via Ethernet cable
   - Connect to the Gear 360's WiFi AP via WiFi
   - Your computer can use both connections simultaneously

2. **Use a USB WiFi adapter:**
   - Use your built-in WiFi for the Gear 360 AP
   - Use a USB WiFi adapter for your regular internet connection
   - macOS/Windows can manage multiple network interfaces

3. **Use your phone's hotspot:**
   - Connect your computer to your phone's WiFi hotspot for internet
   - Use a USB WiFi adapter for the Gear 360 AP

4. **Network bridging (Advanced):**
   - Set up network bridging/routing on your system
   - Requires advanced network configuration knowledge

## Running the Test Application

```bash
./build/bin/gear360_test
```

**Before running:**
- Ensure your computer is connected to the Gear 360's WiFi network
- The camera should be in streaming mode
- The app will automatically connect and start streaming
