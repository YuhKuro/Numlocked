
#ifndef CONFIG_H
#define CONFIG_H

#include "stdint.h"
#include <stdbool.h>
#include "pico/stdlib.h"



// GPIO PIN DEFINITIONS

#define USB_BOOT 0      


#define RIGHT_CHECK 12
#define NUMPAD_CHECK 13
#define NUM_LOCK_SCROLL_LOCK_LED 2
#define CAPS_LOCK_LED 3

#define NUM_ENC_A_I2C_SDA 4
#define NUM_ENC_B_I2C_SCL 5

#define MAIN_ENC_SW 18
#define MAIN_ENC_A 20
#define MAIN_ENC_B 19


#define SHIFT_REG_SH_LD 10
#define SHIFT_REG_CLK 11
#define SHIFT_REG_SERIAL_OUT 1

#define OLED_SDA 22
#define OLED_SCL 23

// END GPIO PIN DEFINITIONS

#define TOTAL_BITS 120 //The total number of keys on the keyboard. 8-bits x 15 shift regs = 120 bits.





#define WPM_CALC_INTERVAL_MS 5000  // Interval to calculate WPM in milliseconds (e.g., 5 seconds)
#define CHARACTERS_PER_WORD 5

#define IDLE_SPEED 10  // below this wpm value the animation will idle

#define ACTIVE_SPEED 40  // above this wpm value typing animation to trigger

#define TIME_BUFFER_SIZE 8

#define QUEUE_SIZE 10


typedef struct {
    uint32_t last_interrupt_time;
    uint32_t characters_typed;      // Counts characters typed
    uint32_t last_wpm_calc_time;    // Last time WPM was calculated
    volatile int wpm;               // Words per minute
    uint8_t currentFrame;
    bool gameMode;
    volatile bool rightSideConnected;
    volatile bool numpadConnected;
    volatile uint8_t lastEncoderState;
    bool scrollNumPWM;
    int bufferFrames;
    
} globalVariables;

extern globalVariables global; 



#endif