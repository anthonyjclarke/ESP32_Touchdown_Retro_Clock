# ESP32 Flash Memory Partitioning Guide

This guide explains the ESP32 flash memory layout, current partition usage, and how to repartition if needed.

## Current Memory Status (v2.1.0)

### Flash Memory Usage Summary

| Component | Allocated | Used | Available | Usage |
|-----------|-----------|------|-----------|-------|
| **Program (App0)** | 1.25 MB | 1.01 MB | 245 KB | **80.9%** |
| **OTA Backup (App1)** | 1.25 MB | 0 MB | 1.25 MB | 0% |
| **LittleFS** | 1.38 MB | 36 KB | 1.34 MB | **2.6%** |
| **Runtime (RAM)** | 320 KB | 60 KB | 260 KB | **18.6%** |

### Key Observations

- **Program space is the constraint** at 80.9% usage (245 KB free)
- **Filesystem is barely used** at 2.6% usage (1.34 MB free)
- **RAM has plenty of headroom** at 18.6% usage (260 KB free)
- **OTA updates are still viable** but approaching practical limit (85-90%)

### Filesystem Contents

Current web interface files (36 KB total):
- `index.html`: 11 KB
- `app.js`: 16 KB
- `style.css`: 3.2 KB

**Available filesystem space**: 1.34 MB (enough for images, fonts, additional pages, data files)

## ESP32 Flash Memory Layout (4MB Total)

### Complete Partition Table

```
Partition         Allocated    Purpose
-------------------------------------------------
Bootloader        4 KB         ESP32 bootloader
Partition Table   4 KB         Partition definitions
NVS               24 KB        Settings storage (runtime)
OTA Data          8 KB         OTA state tracking
App0 (Program)    1.25 MB      Active firmware
App1 (OTA)        1.25 MB      OTA staging area
LittleFS          1.38 MB      Web files & data
Unallocated       88 KB        Reserved
-------------------------------------------------
Total:            4 MB         (4,194,304 bytes)
```

### Current Partition Scheme (default.csv)

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x140000,
app1,     app,  ota_1,   0x150000,0x140000,
spiffs,   data, spiffs,  0x290000,0x160000,
```

**Sizes in decimal:**
- App0: 1,310,720 bytes (1.25 MB)
- App1: 1,310,720 bytes (1.25 MB)
- LittleFS: 1,441,792 bytes (1.38 MB)

## Repartitioning Options

### When to Consider Repartitioning

Consider repartitioning if:
1. Program space usage exceeds 85% (currently at 80.9%)
2. You're adding large features or libraries
3. OTA updates start failing due to space constraints
4. You don't need OTA capability and want maximum program space

### ⚠️ Important Warnings

**Repartitioning will:**
- ✗ Erase ALL settings (NVS gets reformatted)
- ✗ Reset device to factory defaults
- ✗ Require reflashing both firmware and filesystem
- ✗ Require reconfiguring WiFi, timezone, and all settings

**Before repartitioning:**
1. Document your current settings (timezone, NTP server, LED configuration, etc.)
2. Back up any important data
3. Test the new partition on a development device first

## Option 1: Custom Partition with OTA (Recommended)

**Best balance** - More program space while keeping OTA functionality.

### Partition Table: `partitions_custom.csv`

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x1A0000,
app1,     app,  ota_1,   0x1B0000,0x1A0000,
spiffs,   data, spiffs,  0x350000,0xA0000,
```

**Benefits:**
- App0: 1.625 MB (1,664 KB) - **+384 KB more than current**
- App1: 1.625 MB (1,664 KB) - OTA still works
- LittleFS: 640 KB - More than enough for web files (17x current usage)
- **Keeps OTA functionality**

**New capacity:**
- Total program space available: ~630 KB (vs current 245 KB)
- Still room for 17x more web files before filesystem is full

### Implementation Steps

1. **Create the partition file:**
   ```bash
   # Create partitions_custom.csv in project root
   cat > partitions_custom.csv << 'EOF'
   # Name,   Type, SubType, Offset,  Size, Flags
   nvs,      data, nvs,     0x9000,  0x5000,
   otadata,  data, ota,     0xe000,  0x2000,
   app0,     app,  ota_0,   0x10000, 0x1A0000,
   app1,     app,  ota_1,   0x1B0000,0x1A0000,
   spiffs,   data, spiffs,  0x350000,0xA0000,
   EOF
   ```

2. **Update `platformio.ini`:**
   ```ini
   [env:esp32_touchdown]
   board_build.partitions = partitions_custom.csv
   ```

3. **Flash firmware and filesystem:**
   ```bash
   # Upload firmware with new partition table
   pio run -e esp32_touchdown --target upload --upload-port /dev/cu.usbserial-0001

   # Upload filesystem to new partition
   pio run -e esp32_touchdown --target uploadfs --upload-port /dev/cu.usbserial-0001
   ```

4. **Reconfigure device:**
   - Connect to WiFi (device will start in AP mode)
   - Set timezone, NTP server, LED preferences via web interface

## Option 2: Aggressive Custom Partition

**Maximum app space** while keeping OTA - Use if you need even more program space.

### Partition Table: `partitions_aggressive.csv`

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x1E0000,
app1,     app,  ota_1,   0x1F0000,0x1E0000,
spiffs,   data, spiffs,  0x3D0000,0x20000,
```

**Benefits:**
- App0: 1.875 MB (1,920 KB) - **+640 KB more than current**
- App1: 1.875 MB (1,920 KB) - OTA still works
- LittleFS: 128 KB - Still 3.5x your current usage
- **Keeps OTA functionality**

**New capacity:**
- Total program space available: ~890 KB (vs current 245 KB)
- Room for 3.5x more web files

**Implementation:** Same as Option 1, using `partitions_aggressive.csv` instead.

## Option 3: Huge App (No OTA)

**Maximum program space** - Use if you don't need OTA updates.

### Built-in Partition: `huge_app.csv`

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x300000,
spiffs,   data, spiffs,  0x310000,0xE0000,
```

**Benefits:**
- App0: 3 MB (3,072 KB) - **2.4x more space than current**
- LittleFS: 896 KB - Still 25x your current usage
- Simple to implement (built-in partition scheme)

**Tradeoffs:**
- ✗ No OTA updates - must flash via USB
- ✗ No App1 partition

### Implementation

Update `platformio.ini`:
```ini
[env:esp32_touchdown]
board_build.partitions = huge_app.csv
```

Then flash as shown in Option 1, step 3.

## Comparison Table

| Scheme | App Size | OTA Support | Filesystem | Best For |
|--------|----------|-------------|------------|----------|
| **Current (default)** | 1.25 MB | ✓ Yes | 1.38 MB | Balanced, OTA critical |
| **Custom (Recommended)** | 1.625 MB | ✓ Yes | 640 KB | More features + OTA |
| **Aggressive** | 1.875 MB | ✓ Yes | 128 KB | Maximum features + OTA |
| **Huge App** | 3 MB | ✗ No | 896 KB | Maximum features, USB only |

## Partition Scheme Selection Guide

### Stick with Current (default.csv) if:
- You have 245 KB free program space (sufficient for your needs)
- OTA updates are critical
- You might add more web content in the future
- You prefer not to reflash and reconfigure

### Use Custom Partition if:
- You need more program space (384-640 KB extra)
- You want to keep OTA functionality
- You don't plan to add many more web files
- You're comfortable reflashing the device

### Use Huge App if:
- You need maximum program space (2.4x current)
- OTA updates are not important
- USB access is readily available
- You have many features to add

## Monitoring Usage

### Check Program Size
```bash
pio run -e esp32_touchdown 2>&1 | grep -A 2 "Checking size"
```

Output shows:
```
RAM:   [==        ]  18.6% (used 61044 bytes from 327680 bytes)
Flash: [========  ]  80.9% (used 1060097 bytes from 1310720 bytes)
```

### Check Filesystem Size
```bash
# Build filesystem to see partition size
pio run -e esp32_touchdown --target buildfs

# Check filesystem image size
ls -lh .pio/build/esp32_touchdown/littlefs.bin

# Check actual data size
du -sh data/
```

### Calculate Available Space
```bash
python3 << 'EOF'
import os

# Get sizes
fs_size = os.path.getsize('.pio/build/esp32_touchdown/littlefs.bin')
data_size = sum(os.path.getsize(os.path.join('data', f)) for f in os.listdir('data'))

print(f"Filesystem partition: {fs_size/1024/1024:.2f} MB")
print(f"Data files: {data_size/1024:.0f} KB")
print(f"Available: {(fs_size - data_size)/1024/1024:.2f} MB")
print(f"Usage: {(data_size/fs_size)*100:.1f}%")
EOF
```

## Troubleshooting

### Build Errors After Repartitioning

**Error: "Firmware too large"**
- Your compiled firmware exceeds the new partition size
- Solution: Remove features, optimize code, or choose larger partition

**Error: "Partition table doesn't fit"**
- Partition offsets or sizes are incorrect
- Solution: Verify partition CSV calculations (use hex calculator)

### Runtime Issues After Repartitioning

**Device won't boot / boot loop**
- Partition table may not have been flashed correctly
- Solution: Erase flash completely and reflash:
  ```bash
  esptool.py --port /dev/cu.usbserial-0001 erase_flash
  pio run -e esp32_touchdown --target upload
  pio run -e esp32_touchdown --target uploadfs
  ```

**Settings not saving**
- NVS partition may be corrupt
- Solution: Factory reset or erase NVS:
  ```bash
  # Via web interface: Reset WiFi button
  # Or reflash with erase
  ```

**OTA updates fail**
- Not enough space for new firmware in OTA partition
- Solution: Ensure your firmware fits in the App1 partition size

## References

- [ESP32 Partition Tables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html)
- [PlatformIO Board Build Options](https://docs.platformio.org/en/latest/platforms/espressif32.html#partition-tables)
- [ESP32 Flash Layout](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html#built-in-partition-tables)

## Related Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md) - System architecture overview
- [CODEBASE_REVIEW.md](CODEBASE_REVIEW.md) - Code structure and analysis
- [../README.md](../README.md) - Project documentation

---

**Last Updated:** January 2026 (v2.1.0)
**Status:** Current partition scheme has 245 KB program space remaining (80.9% usage)
