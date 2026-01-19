# ESP32 OTA (Over-The-Air) Update Setup Guide

This guide explains how to implement and use OTA updates for ESP32 projects using PlatformIO and Arduino framework.

## What is OTA?

OTA (Over-The-Air) updates allow you to upload new firmware to your ESP32 wirelessly over WiFi, without needing a USB connection. This is especially useful for devices that are:
- Mounted in hard-to-reach locations
- Deployed in the field
- Housed in sealed enclosures

## Quick Setup for New Projects

### 1. Copy the OTA Files

Copy these files to your project:
```
src/ota/
  ├── OtaService.h
  └── OtaService.cpp
```

### 2. Configure platformio.ini

Add the following to your `platformio.ini`:

```ini
[env:your_board_name]
platform = espressif32
board = your_board
framework = arduino

lib_deps = 
    ; ... your other libraries

; OTA Configuration
upload_protocol = espota
upload_port = 192.168.x.x          ; Your ESP32's IP address
upload_flags = 
    --host_port=8266                ; Fixed port to avoid firewall issues
    ; --auth=yourpassword           ; Optional: Add password protection
```

**Important Notes:**
- Replace `192.168.x.x` with your ESP32's actual IP address
- The fixed `host_port=8266` is crucial for Windows Firewall compatibility
- For first upload, comment out `upload_protocol = espota` and use USB

### 3. Initialize in Your Code

Add OTA to your main code:

```cpp
#include "ota/OtaService.h"

void setup() {
    Serial.begin(115200);
    
    // Connect to WiFi first (OTA requires WiFi)
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    
    // Initialize OTA service
    otaInit();
}

void loop() {
    // Handle OTA updates (must be called regularly)
    otaHandle();
    
    // Your application code here
    delay(10);
}
```

## Windows Firewall Configuration

OTA requires Windows Firewall to allow incoming connections from your ESP32.

### Option 1: PowerShell (Recommended)

Run **PowerShell as Administrator** and execute:

```powershell
New-NetFirewallRule -DisplayName "ESP32 OTA Port" -Direction Inbound -Protocol TCP -LocalPort 8266 -Action Allow -Profile Any
```

### Option 2: GUI Method

1. Press `Win + R`, type `wf.msc`, press Enter
2. Click **"Inbound Rules"** → **"New Rule..."**
3. Select **"Port"** → Next
4. Select **"TCP"**, enter **"8266"** → Next
5. Select **"Allow the connection"** → Next
6. Check all profiles (Domain, Private, Public) → Next
7. Name: **"ESP32 OTA Port"** → Finish

### Verify Firewall Rule

Check if the rule was created correctly:

```powershell
Get-NetFirewallRule -DisplayName "*ESP32*" | Format-Table DisplayName, Enabled, Direction, Action
```

## Upload Workflow

### First Upload (Via USB)

1. Comment out OTA settings in `platformio.ini`:
   ```ini
   ;upload_protocol = espota
   ;upload_port = 192.168.x.x
   ```

2. Connect ESP32 via USB

3. Upload:
   ```bash
   pio run --target upload
   ```

4. Monitor serial output to get the IP address:
   ```bash
   pio device monitor
   ```

### Subsequent Uploads (Via OTA)

1. Uncomment OTA settings in `platformio.ini`

2. Update the IP address if needed

3. **Disconnect serial monitor** (important!)

4. Upload wirelessly:
   ```bash
   pio run --target upload
   ```

5. The ESP32 will automatically reboot with the new firmware

## Optional Enhancements

### Add Password Protection

In `OtaService.cpp`, add before `ArduinoOTA.begin()`:

```cpp
ArduinoOTA.setPassword("your_secure_password");
```

And in `platformio.ini`:

```ini
upload_flags = 
    --host_port=8266
    --auth=your_secure_password
```

### Use mDNS Hostname

Instead of IP address, use a hostname:

In `OtaService.cpp`:
```cpp
ArduinoOTA.setHostname("ESP32-DeviceName");
```

In `platformio.ini`:
```ini
upload_port = ESP32-DeviceName.local
```

### Custom Port

Change the OTA port if 8266 conflicts with other services:

In `OtaService.cpp`:
```cpp
ArduinoOTA.setPort(3232);  // or any other port
```

In `platformio.ini`:
```ini
upload_flags = 
    --host_port=3232
    --port=3232
```

Don't forget to update the firewall rule for the new port!

## Troubleshooting

### "No response from device" Error

**Causes:**
- ESP32 is not on the network
- Wrong IP address in `platformio.ini`
- Windows Firewall blocking connection
- Serial monitor is connected (disconnect it!)
- ESP32 not running OTA-enabled firmware

**Solutions:**
1. Ping the ESP32: `ping 192.168.x.x`
2. Verify firewall rule exists
3. Disconnect serial monitor before upload
4. Check ESP32 serial output for OTA initialization message
5. Try uploading via USB first

### Connection Drops During Upload

**Causes:**
- WiFi interference
- ESP32 running resource-intensive tasks
- Not calling `otaHandle()` frequently enough

**Solutions:**
1. Call `otaHandle()` in loop without long delays
2. Stop other tasks during OTA (see callbacks in OtaService.cpp)
3. Improve WiFi signal strength

### Firewall Still Blocking

Some antivirus software may also block connections. Try:

**Add Python.exe to firewall:**
```powershell
New-NetFirewallRule -DisplayName "PlatformIO Python" -Direction Inbound -Action Allow -Program "C:\Users\YourUser\.platformio\penv\Scripts\python.exe" -Profile Any
```

**Or add to Windows Defender exclusions:**
```powershell
Add-MpPreference -ExclusionProcess "python.exe"
```

## How OTA Works

### Two-Phase Process:

1. **Invitation Phase (UDP)**:
   - PC sends invitation to ESP32:3232 via UDP
   - ESP32 receives invitation and prepares to receive firmware

2. **Upload Phase (TCP)**:
   - ESP32 connects back to PC on the specified host port (8266)
   - PC sends firmware binary
   - ESP32 writes firmware to flash
   - ESP32 reboots with new firmware

### Why Fixed Port?

The default behavior uses **random ports**, which Windows Firewall blocks. Using a **fixed port (8266)** allows:
- Creating a permanent firewall rule
- Consistent, reliable connections
- No need to modify firewall rules for each upload

## Security Considerations

1. **Use password protection** in production environments
2. **Disable OTA** after deployment if updates are not needed
3. **Use HTTPS/TLS** for production (requires modifications)
4. **Restrict to specific WiFi networks** using firewall rules
5. **Monitor failed OTA attempts** in your logs

## Benefits of This Implementation

✅ **Reusable** - Copy to any ESP32 project  
✅ **Reliable** - Fixed port eliminates firewall issues  
✅ **Informative** - Progress and error callbacks  
✅ **Flexible** - Easy to add password, hostname, custom port  
✅ **Windows-friendly** - Designed to work with Windows Firewall  

## Reference Files

- `src/ota/OtaService.h` - OTA service header
- `src/ota/OtaService.cpp` - OTA implementation with ArduinoOTA
- `platformio.ini` - OTA upload configuration
- `src/main.cpp` - Example usage

## Additional Resources

- [PlatformIO OTA Documentation](https://docs.platformio.org/en/latest/platforms/espressif32.html#over-the-air-ota-update)
- [ArduinoOTA Library](https://github.com/espressif/arduino-esp32/tree/master/libraries/ArduinoOTA)
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)

---

**Created:** January 2026  
**Project:** EV-ESP32-energimonitor  
**Author:** Steen Bygvraa Vestergaard
