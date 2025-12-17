#include "config.h"
#include "keyboard.h"
#include "keymap.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include <tusb.h>
#include "usb_descriptors.h"
#include <string.h>
#include <stdio.h>
#include "hardware/gpio.h"
#include "bsp/board_api.h"
#include "oled_cdc.h"


//static uint8_t last_state = 0b11;  // Initialize with the idle state (11)
    
/* Called when the encoder is rotated in clockwise or counterclockwise direction. */



typedef struct {
    uint8_t lastEightSamples;               //00000101
    bool held;
    uint8_t initialPress;                   //00: not pressed 01: first press 10: already read.
} keyStatus_t;




bool is_queue_empty(queue_t* q){
    return q->count == 0;
}

bool is_queue_full (queue_t* q) {
    return q->count == QUEUE_SIZE; 
}

void enqueue (queue_t* q, uint8_t input) {
    if (is_queue_full(q)) return;
    q->items[q->end] = input;
    q->end = (q->end + 1) % QUEUE_SIZE;
    q->count++;
}

uint8_t dequeue_when_ready(queue_t* q, uint32_t currentCycle) {
    if (is_queue_empty(q)) return -1;

    if (((currentCycle - q->enqueue_cycle[q->start]) & 31) != 0){
        return -1; // Not ready yet
    }

    uint8_t output = q->items[q->start];
    q->start = (q->start + 1) % QUEUE_SIZE;
    q->count--;
    return output;
}

keyStatus_t keyStatus[TOTAL_BITS] = {0};

queue_t keyQueue = {0};


static void calculate_wpm(uint32_t current_time) {

    if (keyboard.last_wpm_calc_time == 0) {
        keyboard.last_wpm_calc_time = current_time;
        return;
    }
    uint32_t time_elapsed_ms = current_time - keyboard.last_wpm_calc_time;

    
    if (time_elapsed_ms >= WPM_CALC_INTERVAL_MS) {
        if (keyboard.characters_typed > 0) {
            // Calculate WPM only if new characters were typed
            double time_elapsed_min = (double)time_elapsed_ms / 60000.0;  // Convert ms to minutes
            double current_wpm = ((double)keyboard.characters_typed / CHARACTERS_PER_WORD) / time_elapsed_min;

            // Update WPM directly with new value, no rolling average
            global.wpm = (uint32_t)current_wpm;

            // Reset characters_typed for the next interval
            keyboard.characters_typed = 0;
        } else {
            // No new characters typed: Decay WPM towards zero
           if (global.wpm > 0) {
                int decay_amount = 5; // Amount to decay per interval
                global.wpm = (global.wpm > decay_amount) ? global.wpm - decay_amount : 0;
            }
        }

        // Update the last calculation time
        keyboard.last_wpm_calc_time = current_time;
    }
}






static bool key_is_modifier(uint8_t key) {
    return (key >= HID_KEY_CONTROL_LEFT && key <= HID_KEY_GUI_RIGHT);
}

// Function to get the corresponding bit for a modifier key
static uint8_t key_modifier_bit(uint8_t key) {
    uint8_t bit_pos = key - HID_KEY_CONTROL_LEFT;
    if (bit_pos >= 8) return 0; // Safety check
    return 1 << bit_pos;
}
bool get_keyboard_status(uint8_t* key_report, size_t len){

    static uint32_t cycle = 0;
    cycle++;
    if (to_ms_since_boot(get_absolute_time()) < keyboard.quietUntil) {
        memset(key_report, 0, len);
        return false;
    }

    bool keyState[TOTAL_BITS] = {0};

    int keyIndex = 2;  // Start filling regular key codes from the third byte
    bool anyKeyPressed = false;  // Flag to check if any key is pressed

    if (!is_queue_empty(&keyQueue)) {
        uint8_t item = dequeue_when_ready(&keyQueue, cycle);
        if (item != (uint8_t)-1) {
            key_report[keyIndex++] = item;
            anyKeyPressed = true;
        }
    }

    uint8_t *key_map= (!global.rightSideConnected && global.numpadConnected) ? key_map_noright:key_map_standard;


    int loopIterations = TOTAL_BITS;
    if (!(global.rightSideConnected) && !(global.numpadConnected)) {
        loopIterations = 40;
    } else if ((global.rightSideConnected) && !(global.numpadConnected)){
        loopIterations = 96;
    }

    gpio_put(SHIFT_REG_SH_LD, 0); // Set shift register shift/~load pin low to load in data.
    sleep_us(1);              // Short delay
    gpio_put(SHIFT_REG_SH_LD, 1); // Set  shift register shift/~load pin high to prepare to shift data.
    sleep_us(1);              // Short delay
    
    //This loop shifts the data for n number of keys to read the state of each key.
    for (int i = 0; i < loopIterations; i++) {
        if (keyboard.keyboardStateChanged == true) {
            anyKeyPressed = false;
            keyboard.keyboardStateChanged = false;
            keyboard.quietUntil = to_ms_since_boot(get_absolute_time()) + 500;
            memset(key_report, 0, len);
            return false;
            break;
        }

        bool currentPressed = gpio_get(SHIFT_REG_SERIAL_OUT) ? 0 : 1; // Read the bit, flip it so 0 means not pressed, 1 means pressed.

        uint8_t key = key_map[i];

        if (key == HID_KEY_F20){
            gpio_put(SHIFT_REG_CLK, 1);  // Pulse the clock to shift the data
            sleep_us(1);              // Short delay
            gpio_put(SHIFT_REG_CLK, 0);  // Reset clock to low
            sleep_us(1);              // Short delay
            continue; //skip these. 
        }
        
        // Shift in the new sample
        keyStatus[i].lastEightSamples = ((keyStatus[i].lastEightSamples << 1) | (currentPressed & 1)) & 0xFF;

        // Rising edge detection after debounce
        if (currentPressed && ((keyStatus[i].held) ||((keyStatus[i].lastEightSamples & 0x1F) == 0x1F))) {  
            keyState[i] = true;
            if ((keyStatus[i].initialPress & 0x03) == 0x00) {
                keyStatus[i].initialPress = 0x01; 
            }
        } else if (!currentPressed) {
            keyStatus[i].initialPress = 0x00; 
            keyState[i] = false;

        }


        if (keyState[i]){
            anyKeyPressed = true;

            if (key_is_modifier(key)) { 
                if (!global.gameMode || !(key == HID_KEY_GUI_LEFT || key == HID_KEY_GUI_RIGHT)) { //if game mode is active, skip windows.
                    key_report[0] |= key_modifier_bit(key);
                }
            } else {
                if (keyIndex < 8) {
                    switch (key){
                        case HID_KEY_F13:
                            key_report[0] |= key_modifier_bit(HID_KEY_GUI_LEFT);
                            key_report[keyIndex++] = HID_KEY_SPACE;
                            if ((keyStatus[i].initialPress & 0x03) == 0x01){
                                enqueue(&keyQueue, HID_KEY_GRAVE);
                                keyStatus[i].initialPress = 0x02;
                            } 
                            break;
                        case HID_KEY_F14:
                            if ((keyStatus[i].initialPress & 0x03) == 0x01){
                                keyboard.pauseFlag = true;
                                keyStatus[i].initialPress = 0x02;
                            } 
                            break;
                        case HID_KEY_F15:
                            key_report[0] |= key_modifier_bit(HID_KEY_GUI_LEFT);
                            key_report[0] |= key_modifier_bit(HID_KEY_SHIFT_LEFT);
                            key_report[keyIndex++] = HID_KEY_S;
                            break;
                        case HID_KEY_F16:
                            key_report[keyIndex++] = HID_KEY_SCROLL_LOCK;
                            break;
                        case HID_KEY_F18:
                            //Numpad volume knob. Set flag for pause/play handled by main.
                            if ((keyStatus[i].initialPress & 0x03) == 0x01){
                                keyboard.pauseFlag = true;
                                keyStatus[i].initialPress = 0x02;   
                            }
                            break;
                        case HID_KEY_F17:
                        default: 
                            key_report[keyIndex++] = key;
                             if ((key != HID_KEY_SPACE) && (key != HID_KEY_BACKSPACE) && ((keyStatus[i].initialPress & 0x03) == 0x01))  {
                                keyboard.characters_typed++;
                                keyStatus[i].initialPress = 0x02;
                            }
                        break;

                    }
                  
                }
            }

        }
        gpio_put(SHIFT_REG_CLK, 1);  // Pulse the clock to shift the data
        //sleep_us(1);              // Short delay
        gpio_put(SHIFT_REG_CLK, 0);  // Reset clock to low
        //sleep_us(1);              // Short delay
    }
    if (key_report[0] & key_modifier_bit(HID_KEY_ALT_LEFT) && key_report[2] == HID_KEY_BACKSPACE) {
        global.gameMode = !global.gameMode;
    }

    calculate_wpm(board_millis());
    

    if (keyboard.numEncoderDelta >= ENCODER_STEPS_PER_INDENT){
        //arrow right to forward video, add to keyreport if keyIndex < 8
        if (keyIndex < 8) {
            key_report[keyIndex++] = HID_KEY_ARROW_RIGHT;
            anyKeyPressed = true;
            keyboard.numEncoderDelta = 0;
        }
    } else if (keyboard.numEncoderDelta <= -ENCODER_STEPS_PER_INDENT){
        //arrow left to rewind video, add to keyreport if keyIndex < 8
        if (keyIndex < 8) {
            key_report[keyIndex++] = HID_KEY_ARROW_LEFT;
            anyKeyPressed = true;
            keyboard.numEncoderDelta = 0;
        }
    }


    if (!anyKeyPressed) {
        memset(key_report, 0, len);
        return false;
    }
    
    return anyKeyPressed;

}

