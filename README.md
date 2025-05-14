# arduino-dw1000 (Enhanced Fork)

This is a fork and modernization of the original [thotro/arduino-dw1000](https://github.com/thotro/arduino-dw1000) library for the Decawave DW1000 UWB module. It introduces a centralized `DeviceManager` for robust handling of multi-device setups — especially **multi-anchor, single-tag** scenarios.

---

## 🚧 Project Status

**Active Development** — I got this fork to the point where I could use it complete my capstone project, which used 1 tag and 2 anchors for tracking a moving object in a 3d space: https://github.com/ThomasJM03/CSE450_453DustRunners

---

## 🔧 Enhancements in This Fork

- ✅ **DeviceManager**: Manages anchors and tags with automatic activation/inactivation tracking, duplicate filtering, and reactivation.
- ✅ **Improved Multi-anchor Handling**: Seamless support for ranging from one tag to multiple anchors.(However it currently struggles to add device after the start up sequence)
- ✅ **Inactive Device Monitoring**: Automatically deactivates and recovers devices based on activity.
- ✅ **Enhanced Message Parsing**: More robust and readable message type detection.
- ✅ **Better Logging**: Improved debugging output for UWB interactions.

---

## 💡 What You Can Do With This Fork

- Build reliable ranging networks with one tag and multiple anchors.
- Use `DeviceManager` to track device states and streamline ranging cycles.
- Extend or adapt for mesh or advanced topologies thanks to modular design.

---

## 📦 Installation

**Requirements**: Arduino IDE ≥ 1.6.6 with C++11 support

1. Download this forked version as a ZIP.
2. Open Arduino IDE.
3. Navigate to **Sketch > Include Library > Add .ZIP Library...**
4. Select the ZIP file you downloaded.
5. Examples will be available under **File > Examples > arduino-dw1000**.

---

## 🧱 Library Structure

### Core Components

| Class           | Description                                                                 |
|----------------|-----------------------------------------------------------------------------|
| `DW1000`        | Handles low-level configuration and SPI communication with the DW1000 chip. |
| `DW1000Time`    | Manages UWB-specific timestamp calculations and conversions.                |
| `DW1000Device`  | Encapsulates information about a remote device (anchor/tag).                |
| `DW1000Ranging` | Implements the ranging protocol (sending and receiving UWB messages).       |
| `DW1000Mac`     | MAC-level message formatting.                                               |
| `DeviceManager` | **New**: Centralized registry for remote devices (anchors/tags).            |

---

## 🚀 Usage

### Initialization Example

```cpp
#include <DW1000Ranging.h>

void setup() {
  Serial.begin(115200);

  DW1000Ranging.initCommunication();
  DW1000Ranging.startAsTag("82:17:5B:D5:A9:9A:E2:10", DW1000.MODE_LONGDATA_RANGE_LOWPOWER);

  DW1000Ranging.attachNewRange([](DW1000Device* device) {
    Serial.print("Range to ");
    Serial.print(device->getShortAddress(), HEX);
    Serial.print(" : ");
    Serial.print(device->getRange());
    Serial.println(" m");
  });
}


