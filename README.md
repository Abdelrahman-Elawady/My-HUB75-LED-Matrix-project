# My-HUB75-LED-Matrix-project
An LED matrix project utilizing ESP32 BLE and HUB75 P3 LED matrix


hardware used in this project:
-  2x HUB75 P3 64x64 LED matrix display (128x64 pixels for the whole chain)
-  ESP32 WROOM 32D (2016 original esp32 version) in a devkit-c V4 board
-  2x 2090 uF parallel capacitors connected to the VCC and GND of each LED matrix module
-  16 jumper wires 10cm female to female connecting data pins to esp32
-  HUB75 ribbon cable to connect the two LED chained screens (comes with the screen)
-  power cable to connect both the esp32 and the LED matrices to power supply unit
-  power supply module AC/DC adaptor (10A 5V)

software:
- Bluetooth low energy (BLE)
- "ESP32-HUB75-MatrixPanel-I2S-DMA" library (other options include PXmatrix and smartMatrix)
