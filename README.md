Dash8x8k - Oldschool Dash with max7219 led matrix 
(Speed 3x5 Font & Battery 7x2 Bar)
(automatic star/delta switch routine included)

Dashv7FinalUniresCYDesp32BLEk - The Current TFT Version for ST7789 TFT_ESPI ESP32C3-Supermini
(automatic star/delta switch routine included)

esp32c3 ble server - Main ESC bridge (packet parser included) 
ESP32C3-Supermini (Schematic follows...)

Dashboardserversimk - esp32c3 ESC sim for offline dashboard Test with no Scooter

nanogpiok - io expander with nano. 
blinker indicator input pin, 
highbeam input pin, 
delta relay output pin.
add 'em as you need 'em.

You can use the latest TFT_espi lib, but due a bug in the newer Board Files in Arduino IDE
you have to Select the ESP32 Board Package 2.0.14 and install. upper versions will 
*NOT WORK* with TFT_espi (reboot and blank screeen issue) 
for reference (https://github.com/mboehmerm/Three-IPS-Displays-with-ST7789-170x320-240x280-240x320/blob/main/ESP32_C3/README.md)
