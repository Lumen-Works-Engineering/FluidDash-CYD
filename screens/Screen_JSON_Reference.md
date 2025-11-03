Color Reference (RGB565 in Hex)
For your JSON files, here are common colors:

"0000" - Black

"FFFF" - White

"F800" - Red

"07E0" - Green

"001F" - Blue

"FFE0" - Yellow

"07FF" - Cyan

"F81F" - Magenta

"4A49" - Dark Gray (line separator)

Element Types Reference
rect: Rectangle (filled or outline)

line: Horizontal/vertical line

text: Static label text

dynamic: Text from data source

temp: Temperature display

coord: Coordinate (X/Y/Z/A)

status: Machine status (auto-colored)

progress: Progress bar

graph: Graph placeholder

Data Source Names
For the "data" field:

Coordinates: wposX, wposY, wposZ, wposA, posX, posY, posZ, posA

Temperatures: temp0, temp1, temp2, temp3

Status: machineState, feedRate, spindleRPM

System: psuVoltage, fanSpeed

Network: ipAddress, ssid, deviceName, fluidncIP

How to Create and Upload JSON Files
Option 1: Create on PC, Copy to SD Card
Create monitor.json in a text editor (Notepad, VS Code, etc.)

Remove SD card from CYD

Insert SD card into PC

Copy monitor.json to /screens/ folder

Eject SD card and insert back into CYD

Restart FluidDash