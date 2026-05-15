
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


#define WPM_CALC_INTERVAL_MS 5000  // Interval to calculate WPM in milliseconds
#define CHARACTERS_PER_WORD 5

#define IDLE_SPEED 10  // below this wpm value the animation will idle

#define ACTIVE_SPEED 40  // above this wpm value typing animation to trigger

#define TIME_BUFFER_SIZE 8

#define QUEUE_SIZE 8

#define ENCODER_QUEUE_SIZE 8

#define FRAME_SIZE 574

#define ENCODER_STEPS_PER_INDENT 4
#define ENC_SW_DEBOUNCE_MS 1000


#define R_START 0x0
#define R_CCW_BEGIN 0x1
#define R_CW_BEGIN 0x2
#define R_START_M 0x3
#define R_CW_BEGIN_M 0x4
#define R_CCW_BEGIN_M 0x5
#define DIR_CW 0x10
#define DIR_CCW 0x20


typedef struct {
   
    
    volatile int wpm;               // Words per minute
    
    volatile bool gameMode;

    volatile bool rightSideConnected;
    volatile bool rightSideOLEDChange;

    volatile bool numpadConnected;
    volatile bool numpadOLEDChange;

    volatile bool usb_suspended;
    volatile bool usb_mounted;
    
} globalVariables;

typedef struct {
    int lastMainEncoderState;
    int lastNumEncoderState;
    volatile int8_t mainEncoderDelta;
    volatile int8_t numEncoderDelta;
    bool scrollNumPWM;
    bool pauseFlag;
    bool muteFlag;
    int keyQueueDelay;
    volatile bool keyboardStateChanged;
    uint32_t quietUntil;
    volatile uint32_t characters_typed;      // Counts characters typed
    uint32_t last_wpm_calc_time;    // Last time WPM was calculated

    volatile bool mainEncoderChanged;
    volatile bool numEncoderChanged;
} keyboardVariables;

typedef struct {
    uint8_t currentFrame;

} oledVariables;
extern globalVariables global; 
extern keyboardVariables keyboard; 
extern oledVariables oled; 



#endif