# Numlocked

## A Fully Custom Modular Split Keyboard
![Ortho](pictures/orthoGreen.bmp)
## Directory:
- [Keyboard Hardware](hardware/readme.md)
- [Keyboard Firmware](firmware/readme.md)
- [Keyboard Case and Plate](case/readme.md)
- [Revision 1](Revision1/readme.md)
## Features
### **Keyboard Features:**
#### - **Modular split design**: Split 75% layout, flexible configuration with or without numpad
![Front](pictures/mainViewGreenTogether.bmp)
![Main](pictures/mainViewGreen.bmp)
#### - **Magnetic connectors for quick configuration changes**
![magnets](pictures/splitViewGreen.bmp)
#### - **Hot swappable & Gasket mount design**
![Hotswap](pictures/hotswapGreen.bmp)
#### - **Built-in USB Hub**: 3 USB-A ports (1 vertical, 2 horizontal)
![Rear](pictures/rearViewGreen.bmp)
#### - **Integrated Volume Knobs**: Two customizable rotary encoders
![Encoder](pictures/twoEncodersGreen.bmp)
#### - **128x32 OLED**
![OLED](pictures/OLEDViewGreen.bmp)
#### - **Caps-lock, Num-lock and Scroll-Lock LEDs**
![LEDs](pictures/statusLEDView.png)
#### - **Dual-Core Custom Firmware**

- **Hardware**:
	- **Shift-Registered Design**: Faster scanning than traditional matrix-based keyboards.
	- **RP2040 chip**: Power efficient, endlessly customizable, easy to program.
	- **Real-Time Clock (RTC)**: Keeps accurate time for the user.
- **Keyboard Build**:
	- Akko CS Silver Switches
	- Durock V2 PCB Mount Stabilizers
	- FR4 Plate
	- Red Oak Case

## Build Instructions
*See the [Build Guide](hardware/readme.md) for detailed steps.*
## Customization
Firmware, coded in C, which leverages all the features of this keyboard are provided in this repo. [Learn more](firmware/readme.md)
Please note that QMK/VIA are currently incompatible with shift-register based keyboard designs.
## Often Asked Questions
- Why is it split?
	- For SMD Reflow Soldering, the largest surface area my hotplate could manage was 400x400mm. I also wanted to experiment with ergonomics.
- What was the total cost?
	- Will update once everything is complete.

## Thanks To:
- [SSD1306 library] (https://github.com/daschr/pico-ssd1306) by dachr. I added a function to print both images and text, but it was with his foundations that I was able to quickly get the OLED working.
- [cJSON library] (https://github.com/DaveGamble/cJSON) by DaveGamble. I swear I'm always using this. Its never failed me once. 
