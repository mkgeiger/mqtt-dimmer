# MQTT dimmer
## Overview (Revision unknown)
Instead of building a Wifi dimmer by myself, which is not allways the cheapest and fastest solution, I decided to use a commercial WiFi dimmer. Focus was then more on the software than on the hardware design. The choice fell on the `Luminea NX-4653`, a one channel 230V AC dimmer, which is cheap, easy to order, easy to open and the contained ESP8266 can easily be reflashed. Also getting rid of the original firmware, which makes use of a Chinese MQTT cloud, is a good feeling. Instead, my software connects to a local self maintained MQTT broker (see other project from me).

## Overview REV3_2023_07_13
I used the `Luminea NX-4653-675`. I am going to address it as REV3.

## Hardware
The hardware is the commercial product `Luminea NX-4653`. It is sold by PEARL (https://www.pearl.de/a-NX4653-3103.shtml) and Amazon (https://www.amazon.de/dp/B084P762LH/ref=cm_sw_em_r_mt_dp_ut0aGb4T5BBRB?_encoding=UTF8&psc=1) .
<img src="/photos/nx4653.jpg" alt="drawing" width="400"/>

If you buy it from PEARL as of 26.01.2024 it should be a REV3.
<img src="/photos/nx4653-675.jpg" alt="drawing" width="400"/>


There is an ESP8266 sitting on the microcontroller PCB. Whenever the switch S1 is pressed there are 50/60 Hz pulses detected on the ESP8266 input pin. This phase detection circuit is connected to GPIO13 of the ESP8266 chip. These pulses must be processed as a counting signal. The ESP8266 then sends commands accordingly via the hardware serial interface (UART0) for dimming operation, which are handled by a second microcontroller (STM8S003F3 MCU). The serial communication at 9600 baud is simple, just one command with changing the dimming xx values: `FF 55 xx 05 DC 0A`. Detecting pulses on GPIO13 (S1 pressed) during startup is assigned by my software to reset the Wifi settings.

## Serial connection
The one and only serial connection (UART0) has 2 puposes: 1. communication between ESP8266 and the STM8S003F3 MCU and 2. reflashing the microcontrollers.
The serial header (3.3V, RXD, TXD, GND) as well as GPIO0 are populated as test pads or near to the [LM1](/datasheets/LM1_datasheet.pdf) module (ESP8266) on the microcontroller PCB.

<img src="/hardware/lm1.jpg" alt="drawing" width="400"/>

You can easily add some solder to fix the wires for the flash process. You need to connect to the serial programming interface of the ESP8266 chip. This is done by connecting any serial-to-USB converter (e.g. the FT232R) TXD, RXD and GND pins to the ESP8266 RXD, TXD and GND pins (cross connection!) and powering the NX-4653. Recheck your serial-to-USB converter so to ensure that it supplies 3.3V voltage and NOT 5V. 5V will damage the ESP chip! Do NEVER connect the serial connection to the PC while the dimmer is connected to 230V AC. You risk an electric shock and a damaged PC. ALLWAYS disconnect the dimmer from 230V AC while soldering and flashing!

<img src="/photos/pcb.jpg" alt="drawing" width="400"/>

## Flash mode
Make sure to connect NRST to GND before starting flashing the ESP8266, as this will hold the STM8S003F3 MCU in reset not impacting its flash content while reflashing the ESP8266. To place the board into flashing mode, you will need to short GPIO0 to GND. This can remain shorted while flashing is in progress, but you will need to remove the short in order to boot afterwards the flashed software. Do NEVER connect the serial connection to the PC while the dimmer is connected to 230V AC. You risk an electric shock and a damaged PC. ALLWAYS disconnect the dimmer from 230V AC while soldering and flashing!

## Installation
1. install Arduino IDE 1.8.1x
2. download and install the ESP8266 board supporting libraries with this URL: http://arduino.esp8266.com/stable/package_esp8266com_index.json
3. select the `Lolin(Wemos) D1 mini Lite` board
4. install the `Async MQTT client` library: https://github.com/marvinroger/async-mqtt-client/archive/master.zip
5. install the `Async TCP` library: https://github.com/me-no-dev/ESPAsyncTCP/archive/master.zip
6. compile and flash

## External connection schematic
All dimmable 230V lamps (incl. LED bulbs) up to 150 Watt can used with the dimmer. The wiring diagram is like following:

<img src="/photos/wiring.jpg" alt="drawing" width="300"/>

## SW configuration
The configuration is completely done in the web frontend of the WifiManager. At first startup, the software boots up in access point mode. In this mode you can configure parameters like
* Wifi SSID
* Wifi password
* MQTT broker IP address
* MQTT user
* MQTT password

After these settings were saved, with the next startup, the software boots into normal operating mode and connects to your Wifi and MQTT broker. Entering again into the WifiManager configuration menu can be done be holding the S1 switch pressed during the startup of the software.

## MQTT mode operation
The software subsribes to MQTT topics, over which the dimming value and the on/off value of the NX-4653 can be changed. The changed dimming value is stored to EEPROM to be able to restore it after the next startup (e.g. after a power loss). Also the software supports re-connection to Wifi and to the MQTT broker in case of power loss, Wifi loss or MQTT broker unavailability. The MQTT topics begin with the device specific MAC-address string (in the following "A020A600F73A" as an example). This is useful when having multiple controllers in your MQTT cloud to avoid collisions.

Subscribe topics (used to control the dimming value):
* Topic: "/A020A600F73A/dim"      Payload: "0" - "255"
* Topic: "/A020A600F73A/onoff"    Payload: "0" - "1"

Publish topics (used to give feedback to the MQTTbroker about the dimming value at startup or when changed by switch S1):
* Topic: "/A020A600F73A/dim_fb"   Payload: "0" - "255"

## Manual mode operation
There is no need to control the dimmer over MQTT. Also changing manually the dimming is supported by using switch S1 (GPIO13). The dimming is performed as long the switch S1 is pressed. When the dimming reaches the minimum (0) or maximum (255) value, the dimming stops and the dimming direction (up->down, down->up) is changed. With a subsequent press of the switch S1 it will dimm in the other direction. Releasing the switch S1 will store the actual dimming value to EEPROM and publish it also to the MQTT broker.

## Disassembled microcontoller PCB
This step is not required to perform the instructions of this document. It is possible to flash the ESP8266 also in-circuit. But please again: ALLWAYS disconnect the dimmer from 230V AC while soldering and flashing! I just needed to isolate the microcontoller PCB for the analysis of the electic circuit to find e.g. pin NRST. Here are some pictures of the isolated microcontroller PCB:

<img src="/photos/uc_board.jpg" alt="drawing" width="400"/>

<img src="/photos/uc_board_connected.jpg" alt="drawing" width="400"/>
