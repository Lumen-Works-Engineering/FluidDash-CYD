Verbose mode can be enabled via `-v, --verbose` option
CONFIGURATION: https://docs.platformio.org/page/boards/espressif32/esp32dev.html
PLATFORM: Espressif 32 (6.12.0) > Espressif ESP32 Dev Module
HARDWARE: ESP32 240MHz, 320KB RAM, 4MB Flash
DEBUG: Current (cmsis-dap) External (cmsis-dap, esp-bridge, esp-prog, iot-bus-jtag, jlink, minimodule, olimex-arm-usb-ocd, olimex-arm-usb-ocd-h, olimex-arm-usb-tiny-h, olimex-jtag-tiny, tumpa)
PACKAGES:
 - framework-arduinoespressif32 @ 3.20017.241212+sha.dcc1105b
 - tool-esptoolpy @ 2.40900.250804 (4.9.0)
 - tool-mkfatfs @ 2.0.1
 - tool-mklittlefs @ 1.203.210628 (2.3)
 - tool-mkspiffs @ 2.230.0 (2.30)
 - toolchain-xtensa-esp32 @ 8.4.0+2021r2-patch5
LDF: Library Dependency Finder -> https://bit.ly/configure-pio-ldf
LDF Modes: Finder ~ deep+, Compatibility ~ soft
Found 41 compatible libraries
Scanning dependencies...
Dependency Graph
|-- RTClib @ 2.1.4
|-- LovyanGFX @ 1.2.7
|-- WiFiManager @ 2.0.17+sha.32655b7
|-- WebSockets @ 2.7.1
|-- ArduinoJson @ 7.4.2
|-- OneWire @ 2.3.8
|-- DallasTemperature @ 3.11.0
|-- Preferences @ 2.0.0
|-- WiFi @ 2.0.0
|-- SD @ 2.0.0
|-- FS @ 2.0.0
|-- SPI @ 2.0.0
|-- Adafruit BusIO @ 1.17.4
|-- Wire @ 2.0.0
|-- LittleFS @ 2.0.0
|-- Update @ 2.0.0
|-- WebServer @ 2.0.0
|-- DNSServer @ 2.0.0
|-- ESPmDNS @ 2.0.0
|-- WiFiClientSecure @ 2.0.0
Building in release mode
Linking .pio\build\esp32dev\firmware.elf
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/html_pages.cpp.o: in function `getMainHTML()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/html_pages.cpp:13: multiple definition of `getMainHTML()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:1350: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/html_pages.cpp.o: in function `getSettingsHTML()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/html_pages.cpp:25: multiple definition of `getSettingsHTML()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:1361: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/html_pages.cpp.o: in function `getAdminHTML()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/html_pages.cpp:57: multiple definition of `getAdminHTML()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:1392: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/html_pages.cpp.o: in function `getWiFiConfigHTML()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/html_pages.cpp:73: multiple definition of `getWiFiConfigHTML()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:1407: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_api.cpp.o: in function `getStatusJSON()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_api.cpp:34: multiple definition of `getStatusJSON()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:1451: first 
defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_api.cpp.o: in function `getConfigJSON()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_api.cpp:15: multiple definition of `getConfigJSON()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:1436: first 
defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_handlers.cpp.o: in function `handleRoot()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_handlers.cpp:31: multiple definition of `handleRoot()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:955: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_handlers.cpp.o: in function `handleSettings()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_handlers.cpp:36: multiple definition of `handleSettings()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:959: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_handlers.cpp.o: in function `handleAdmin()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_handlers.cpp:41: multiple definition of `handleAdmin()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:963: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_handlers.cpp.o: in function `handleWiFi()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_handlers.cpp:46: multiple definition of `handleWiFi()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:967: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_handlers.cpp.o: in function `handleUpload()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_handlers.cpp:51: multiple definition of `handleUpload()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:1149: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_handlers.cpp.o: in function `handleEditor()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_handlers.cpp:86: multiple definition of `handleEditor()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:1294: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_handlers.cpp.o: in function `handleAPIConfig()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_handlers.cpp:101: multiple definition of `handleAPIConfig()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:971: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_handlers.cpp.o: in function `handleAPIStatus()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_handlers.cpp:106: multiple definition of `handleAPIStatus()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:975: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_handlers.cpp.o: in function `handleAPIRTC()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_handlers.cpp:111: multiple definition of `handleAPIRTC()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:1091: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_handlers.cpp.o: in function `handleUploadStatus()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_handlers.cpp:116: multiple definition of `handleUploadStatus()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:1251: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_handlers.cpp.o: in function `handleGetJSON()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_handlers.cpp:121: multiple definition of `handleGetJSON()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:1265: 
first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_handlers.cpp.o: in function `handleAPISave()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_handlers.cpp:129: multiple definition of `handleAPISave()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:979: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_handlers.cpp.o: in function `handleAPIAdminSave()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_handlers.cpp:174: multiple definition of `handleAPIAdminSave()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:1014: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_handlers.cpp.o: in function `handleAPIResetWiFi()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_handlers.cpp:201: multiple definition of `handleAPIResetWiFi()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:1035: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_handlers.cpp.o: in function `handleAPIRestart()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_handlers.cpp:209: multiple definition of `handleAPIRestart()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:1042: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_handlers.cpp.o: in function `handleAPIWiFiConnect()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_handlers.cpp:224: multiple definition of `handleAPIWiFiConnect()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:1048: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_handlers.cpp.o: in function `handleAPIReloadScreens()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_handlers.cpp:260: multiple definition of `handleAPIReloadScreens()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:1080: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_handlers.cpp.o: in function `handleAPIRTCSet()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_handlers.cpp:272: multiple definition of `handleAPIRTCSet()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:1107: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_handlers.cpp.o: in function `handleUploadJSON()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_handlers.cpp:349: multiple definition of `handleUploadJSON()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:1188: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_handlers.cpp.o: in function `handleUploadComplete()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_handlers.cpp:414: multiple definition of `handleUploadComplete()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:1241: first defined here
c:/users/john_sparks/.platformio/packages/toolchain-xtensa-esp32/bin/../lib/gcc/xtensa-esp32-elf/8.4.0/../../../../xtensa-esp32-elf/bin/ld.exe: .pio/build/esp32dev/src/web/web_handlers.cpp.o: in function `handleSaveJSON()':
C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/web/web_handlers.cpp:315: multiple definition of `handleSaveJSON()'; .pio/build/esp32dev/src/main.cpp.o:C:\Users\john_sparks\Documents\PlatformIO\Projects\FluidDash-CYD/src/main.cpp:1270: first defined here
collect2.exe: error: ld returned 1 exit status
*** [.pio\build\esp32dev\firmware.elf] Error 1
================================================= [FAILED] Took 59.10 seconds =====