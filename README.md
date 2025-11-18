# PHAC Firmware
## Key Features

####  Dual-Mode Operation
- **Keyboard/Mouse Mode**: 7 customizable buttons + dual-knob mouse control
- **Gamepad Mode**: HID gamepad with dual analog axes

####  Advanced Features
- **Key Remapping**: Online configuration via web tool ([https://keymapper.phdesign.cc/](https://keymapper.phdesign.cc/))
- **START Button Protection**: Configurable protection time (100ms/250ms/500ms) to prevent accidental presses
- **Macro Support**: Triple-click START to trigger custom macro sequences
- **RGB Lighting**: Independent RGB control for each button

####  Hardware Features
- **Dual Rotary Encoders**: High-precision quadrature decoding with PIO
- **Button Debouncing**: Hardware-level debouncing with multiple modes
- **WS2812B RGB**: DMA-driven efficient RGB LED control
- **USB HID**: Multi-interface device (Keyboard + Mouse + Gamepad + Raw HID)


>  Modify pin assignments in `config.h` to match your hardware

###  Quick Start

#### Prerequisites

1. **Install Pico SDK**
   ```bash
   # Recommended: Use VS Code Raspberry Pi Pico extension
   # It automatically installs SDK, toolchain, and picotool
   ```

2. **Clone Repository**
   ```bash
   git clone https://github.com/ph-design/PHAC_Firmware.git
   cd PHAC_Firmware
   ```

#### Build Firmware

**Method 1: VS Code (Recommended)**
1. Install "Raspberry Pi Pico" extension
2. Open project folder
3. Press `Ctrl+Shift+P`, select `Raspberry Pi Pico: Compile Project`
4. Find `build/PHAC-Firmware.uf2` after compilation

#### Flash Firmware

**Method 1: UF2 Upload (Easiest)**
1. Hold BOOTSEL button on Pico
2. Connect USB cable to PC
3. Pico appears as USB storage device
4. Drag `PHAC-Firmware.uf2` to the device
5. Automatically reboots when complete

###  Key Mapping Configuration

Visit [https://keymapper.phdesign.cc/](PHAC-keymapper) to configure:

- ✅ Keyboard mode key mapping
- ✅ Gamepad mode button mapping
- ✅ Custom RGB colors
- ✅ START button protection time
- ✅ Macro configuration
- ✅ One-click save to device

### Mode Switching

**Boot-time Mode Selection**:
- Hold **Button A** + plug USB → Keyboard/Mouse mode
- Hold **Button B** + plug USB → Gamepad mode
- Hold **START** + plug USB → BOOTSEL (flash mode)

**Runtime Switching**:
- Use web configuration tool to switch modes online

###  Project Structure

```
PHAC_Firmware/
├── main.c                  # Main program entry
├── config.h                # Hardware config & defaults
├── CMakeLists.txt          # CMake build config
├── modules/                # Function modules
│   ├── debounce/          # Button debouncing
│   ├── encoder/           # EC11 encoder (PIO)
│   ├── remap/             # Key mapping & config
│   ├── rgb/               # WS2812 RGB control
│   └── usb/               # USB HID descriptors
├── generated/             # PIO generated headers
└── build/                 # Build output
```

### 🗺️ Roadmap

- [x] Basic USB HID communication
- [x] Button debouncing
- [x] PIO encoder driver
- [x] WS2812 RGB support
- [x] Key remapping
- [x] Web configuration tool
- [x] START button protection
- [x] Macro support
- [ ] Key combination mapping
- [ ] More RGB animation modes
- [ ] OLED display support

###  License
This project is licensed under the MIT License - see [LICENSE.TXT](LICENSE.TXT)

###  Contributing

Issues and Pull Requests are welcome!