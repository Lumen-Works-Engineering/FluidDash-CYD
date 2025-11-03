src/
├── main.cpp                    # Core setup/loop only (~200 lines)
├── config/
│   ├── config.h               # Config structures and defaults
│   ├── config.cpp             # Load/save preferences
│   └── pins.h                 # Hardware pin definitions
├── display/
│   ├── display.h              # Display class definition
│   ├── display.cpp            # LovyanGFX setup & core functions
│   ├── screen_renderer.h      # JSON screen rendering
│   ├── screen_renderer.cpp    # Draw functions for all element types
│   └── ui_modes.cpp           # Mode switching & button handling
├── sensors/
│   ├── sensors.h              # Sensor reading interface
│   ├── temperature.cpp        # DS18B20 temperature sensors
│   ├── fan_control.cpp        # PWM fan & tachometer
│   └── psu_monitor.cpp        # PSU voltage monitoring
├── network/
│   ├── wifi_manager.cpp       # WiFi setup & connection
│   ├── web_server.cpp         # AsyncWebServer handlers
│   └── fluidnc_client.cpp     # WebSocket connection to FluidNC
├── storage/
│   ├── sd_card.cpp            # SD card initialization
│   └── json_parser.cpp        # JSON layout file parsing
└── utils/
    ├── rtc.cpp                # Real-time clock functions
    └── watchdog.cpp           # Watchdog timer management
