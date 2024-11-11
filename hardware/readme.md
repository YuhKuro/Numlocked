# Hardware

## Table of Contents
- [Introduction](##introduction)
- [Keyboard Fundamentals](##Keyboard Fundamentals)
- [HUB PCB](#Hub PCB)
- [Leftside PCB](#left-side-pcb)
- [Rightside PCB](#right-side-pcb)
- [Numpad PCB](numpad-pcb)


## Introduction
The keyboard consists of four PCBs: A USB-Hub and three PCBs for each part of the keyboard: Leftside, Rightside and Numpad.
Each component is connected via magnetic pogo pins to transmit Power, Ground, keyboard signals and LED signals.

## Keyboard Fundamentals
Although most keyboards use a matrix-and-diode system, this keyboard uses a Parallel-load Serial-Shift shift-register approach.

Think of it like a train with containers moving along a circular track. Each container represents a key position, and the train can either load or shift its contents.

When the train is in "Load" mode, each container is set based on whether its corresponding key is pressed. If a key is not pressed, the container remains full; if it's pressed, the container will empty its contents out. 

In "Shift" mode, the train moves each container along, allowing us to check them one by one and identify which keys are pressed by checking which containers are empty.

In this keyboard, all shift registers are pulled up to 3V3 through a 10k-ohm resistor, so each key naturally sits at a high (3.3V) state, much like a full train car. When a key is pressed, it ground that bit on the shift register, reading it as low. By shifting the bits in the register the exact number of keys and feeding the information into an array, we will have a snapshot of what keys are pressed in that cycle. 

Since the SN74HC165DR Shift Registers used in this keyboard allow clocking in the range of a few MHz, there is practically zero chance a key input is missed as the next "load" phase is a just a few microseconds away.

This system also allows for minimal debouncing, requires zero diodes, only requires three pins to read any number of keys and true n-key rollover.

### Why this method?
Using shift registers allows for overall less individual soldering, easier programming and most importantly, only requires three pins (SH/~LD, CLK and Serial-Out).
If we chain shift registers beginning with the removable parts, such as the numpad, and transmit the serial-out to the next removable part, such as the right side, and finally to the left-side, we can manage a system with only one MCU by using multiplexers to avoid unknown values. 
![[Muxes-and-Debouncing.png]]
We can do this by using multiplexers, which are basically data switches, and schmitt triggers which help ensure a clear signal. By using the schmitt triggers, the multiplexers always have a clean input on which component is connected. 
The logic table looks like the following:
Is the numpad connected?
Yes: Use the numpad's serial out and connect it to the next multiplexer
No: Use a 3v3 through a 10k-ohm res (a high 1) as a blanked "no keys pressed".
Is the right side connected?
Yes: Use the right side's output as the initial input for the left-side's shift registers. This is because the right-side will always handle the numpad if the numpad is connected.
No: Use the numpad's output directly. The result of the first mux is passed directly here.

The edge case of numpad connected, but no right side is handled in software.

## Hub PCB
The Hub PCB is centered around a one-to-four USB 2.0 Full Speed HUB. There is one upstream USB-C port which connects to the user's computer and three USB-A ports for other peripherals, as well as an FFC cable which connects it to the keyboard itself.

To minimize space used in the case, there is a notched hole on the PCB itself to allow the FFC Cable to connect between the Hub to the keyboard PCB even when the two PCBs may be flush.

![[USB-Hub-Overview.png]]
## Left-Side PCB
The Left-Side PCB serves as the core of the keyboard’s functionality and is the only component capable of operating independently. This is due to the onboard RP2040 microcontroller, which manages all keyboard input processing. The Left-Side PCB features an FFC connector linking it to the USB Hub PCB, enabling seamless integration within the keyboard assembly.
The Left-Side PCB Features:
- FFC Connector to connect to the Hub
- RP2040 Microcontroller
	- 128 Mbit SPI Flash Memory
	- Crystal Oscillator
	- Header pins for Reset and USB Boot
- Five SN74HC165DR parallel-load serial-out shift registers to handle inputs
- Two four-pin magnetic pogo pins (2x4) to connect to the Numpad PCB.
- Three four-pin magnetic pogo pins (3x4) to connect to the Right-Side PCB
- Muxes and Debouncing Schmitt Triggers
- Real-time Clock (RTC) and CR1220 coin battery
### RP2040
The MCU for this keyboard is the RP2040, select for its cost, power efficiency and ease-of-programming, along with its plethora of GPIO pins and IO options. 
![[RP2040-Flash-RTC.png]]
## Right-Side PCB
The Right-side PCB handles the right side of the keyboard's inputs and passes it the left-side of the keyboard.
Furthermore, the Right-Side PCB (if present) will also pass through the Numpad's data to the left side of the keyboard. This design was chosen to minimize the number of Magnetic Pogo Pin Connectors
## Numpad PCB
The Numpad PCB contains much of the same components as the Left-Side PCB.
