# Firmware for Numlocked Keyboard

Firmware used to interface with windows/linux/probably MacOS but I don't have one to test :(

## To Build:

### Prerequisites:
You need the following:
1. [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
2. cmake, python3, a native compiler, and a GCC compiler: ```sudo apt install cmake python3 build-essential gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib```
3. [Picotool](https://github.com/raspberrypi/picotool) is nice to have for flashing

### Commands:

You will likely have to modify the `CMakeLists.txt` file's fifth line which sets the Pico SDK directory. By default, it is set to my setup where the pico-sdk directory is located two directoies above this one.

It is the usual cmake.
```
mkdir build
cd build
cmake ..
make
```

Then, you can use picotool to flash. Move to wherever you installed picotool and:  
```
sudo ./picotool load PARENT/Numlocked/firmware/build/numlocked.uf2

sudo ./picotool verify PARENT/Numlocked/firmware/build/numlocked.uf2

sudo ./picotool reboot
```




