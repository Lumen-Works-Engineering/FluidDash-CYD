# FluidDash-CYD Project Files

Complete list of source code files with direct raw GitHub URLs for easy access and sharing.

## Repository Information

- **Repository**: Lumen-Works-Engineering/FluidDash-CYD
- **Branch**: main
- **Last Updated**: November 6, 2025

---

## Core Project Files

### Main Source File

- **src/main.cpp**  
  https://raw.githubusercontent.com/Lumen-Works-Engineering/FluidDash-CYD/main/src/main.cpp
  
  ### PlatformIO Configuration
- **platformio.ini**  
  https://raw.githubusercontent.com/Lumen-Works-Engineering/FluidDash-CYD/main/platformio.ini

---

## Configuration Module (`src/config/`)

### Header Files

- **config/config.h**  
  https://raw.githubusercontent.com/Lumen-Works-Engineering/FluidDash-CYD/main/src/config/config.h
  
  ### Implementation Files
- **config/config.cpp**  
  https://raw.githubusercontent.com/Lumen-Works-Engineering/FluidDash-CYD/main/src/config/config.cpp

---

## Display Module (`src/display/`)

### Header Files

- **display/display.h**  
  https://raw.githubusercontent.com/Lumen-Works-Engineering/FluidDash-CYD/main/src/display/display.h
  
  ### Implementation Files
- **display/display.cpp**  
  https://raw.githubusercontent.com/Lumen-Works-Engineering/FluidDash-CYD/main/src/display/display.cpp

---

## Network Module (`src/network/`)

### Header Files

- **network/network.h**  
  https://raw.githubusercontent.com/Lumen-Works-Engineering/FluidDash-CYD/main/src/network/network.h
  
  ### Implementation Files
- **network/network.cpp**  
  https://raw.githubusercontent.com/Lumen-Works-Engineering/FluidDash-CYD/main/src/network/network.cpp

---

## Sensors Module (`src/sensors/`)

### Header Files

- **sensors/sensors.h**  
  https://raw.githubusercontent.com/Lumen-Works-Engineering/FluidDash-CYD/main/src/sensors/sensors.h
  
  ### Implementation Files
- **sensors/sensors.cpp**  
  https://raw.githubusercontent.com/Lumen-Works-Engineering/FluidDash-CYD/main/src/sensors/sensors.cpp

---

## Utilities Module (`src/utils/`)

### Header Files

- **utils/utils.h**  
  https://raw.githubusercontent.com/Lumen-Works-Engineering/FluidDash-CYD/main/src/utils/utils.h
  
  ### Implementation Files
- **utils/utils.cpp**  
  https://raw.githubusercontent.com/Lumen-Works-Engineering/FluidDash-CYD/main/src/utils/utils.cpp

---

## Web Server Module (`src/webserver/`)

### Header Files

- **webserver/webserver.h**  
  https://raw.githubusercontent.com/Lumen-Works-Engineering/FluidDash-CYD/main/src/webserver/webserver.h
  
  ### Implementation Files
- **webserver/webserver.cpp**  
  https://raw.githubusercontent.com/Lumen-Works-Engineering/FluidDash-CYD/main/src/webserver/webserver.cpp

---

## Data Files (`data/`)

### Screen Layout JSON Files

- **data/screen_layout.json**  
  https://raw.githubusercontent.com/Lumen-Works-Engineering/FluidDash-CYD/main/data/screen_layout.json
- **data/screen1.json** (if exists)  
  https://raw.githubusercontent.com/Lumen-Works-Engineering/FluidDash-CYD/main/data/screen1.json
- **data/screen2.json** (if exists)  
  https://raw.githubusercontent.com/Lumen-Works-Engineering/FluidDash-CYD/main/data/screen2.json
  
  ### Configuration Files
- **data/config.json** (if exists)  
  https://raw.githubusercontent.com/Lumen-Works-Engineering/FluidDash-CYD/main/data/config.json

---

## Documentation Files

- **README.md**  
  https://raw.githubusercontent.com/Lumen-Works-Engineering/FluidDash-CYD/main/README.md
- **LICENSE** (if exists)  
  https://raw.githubusercontent.com/Lumen-Works-Engineering/FluidDash-CYD/main/LICENSE

---

## How to Use These Links

### For Direct Access

Click any raw GitHub URL above to view or download the file directly.

### For Perplexity.ai or Other AI Tools

Share this document or copy the raw URLs you need. The AI can fetch and analyze the code directly from these URLs.

### For Bulk Download

Use `git clone` to get the entire repository:

```bash
git clone https://github.com/Lumen-Works-Engineering/FluidDash-CYD.git
```

### For Individual File Download via Command Line

```bash
# Example: Download main.cpp
curl -O https://raw.githubusercontent.com/Lumen-Works-Engineering/FluidDash-CYD/main/src/main.cpp
```

---

## Project Structure Overview

```
FluidDash-CYD/
â”œâ”€â”€ src/
â”‚   â”œâ”€â”€ main.cpp              # Main entry point
â”‚   â”œâ”€â”€ config/               # Configuration management
â”‚   â”‚   â”œâ”€â”€ config.h
â”‚   â”‚   â””â”€â”€ config.cpp
â”‚   â”œâ”€â”€ display/              # TFT display handling
â”‚   â”‚   â”œâ”€â”€ display.h
â”‚   â”‚   â””â”€â”€ display.cpp
â”‚   â”œâ”€â”€ network/              # WiFi and network operations
â”‚   â”‚   â”œâ”€â”€ network.h
â”‚   â”‚   â””â”€â”€ network.cpp
â”‚   â”œâ”€â”€ sensors/              # Temperature and sensor readings
â”‚   â”‚   â”œâ”€â”€ sensors.h
â”‚   â”‚   â””â”€â”€ sensors.cpp
â”‚   â”œâ”€â”€ utils/                # Helper functions and utilities
â”‚   â”‚   â”œâ”€â”€ utils.h
â”‚   â”‚   â””â”€â”€ utils.cpp
â”‚   â””â”€â”€ webserver/            # Web interface and API
â”‚       â”œâ”€â”€ webserver.h
â”‚       â””â”€â”€ webserver.cpp
â”œâ”€â”€ data/                     # JSON configuration and screen layouts
â”‚   â””â”€â”€ *.json
â”œâ”€â”€ platformio.ini            # PlatformIO build configuration
â””â”€â”€ README.md                 # Project documentation
```

---

## Notes

- All URLs point to the `main` branch
- Replace `main` with your specific branch name if working on a different branch
- Files marked "(if exists)" should be verified in your repository
- This list reflects the refactored modular structure from the original single main.cpp
  **Generated**: November 6, 2025  
  **For**: FluidDash-CYD ESP32 CNC Monitoring System
