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



//static uint8_t last_state = 0b11;  // Initialize with the idle state (11)
    
/* Called when the encoder is rotated in clockwise or counterclockwise direction. */



typedef struct {
    uint8_t lastEightSamples;               //00000101
    bool held;
} keyStatus_t;

typedef struct {
    uint8_t items[QUEUE_SIZE];
    int start;
    int end;
    int count;
} keyQueue_t;

void initialize_queue(keyQueue_t* q){
    q->start = 0;
    q->end = 0;
    q->count = 0;
}

bool is_queue_empty(keyQueue_t* q){
    return q->count == 0;
}

bool is_queue_full (keyQueue_t* q) {
    return q->count == QUEUE_SIZE; 
}

void enqueue (keyQueue_t* q, uint8_t key) {
    if (is_queue_full(q)) return;
    q->items[q->end] = key;
    q->end = (q->end + 1) % QUEUE_SIZE;
    q->count++;
}

int dequeue(keyQueue_t* q) {
    if (is_queue_empty(q)) return -1;
    uint8_t key = q->items[q->start];
    q->start = (q->start + 1) % QUEUE_SIZE;
    q->count--;
    return key;
}

keyStatus_t keyStatus[TOTAL_BITS] = {0};

keyQueue_t queue = {0};


void encoderCallBack(uint gpio, uint32_t events) {

    static uint32_t lastValidTime = 0;
    uint32_t currentTime = board_millis();

    if ((currentTime - lastValidTime) < 20) {
        return;
    }
    lastValidTime = currentTime;


    uint8_t enc_a_state = gpio_get(MAIN_ENC_A);
    uint8_t enc_b_state = gpio_get(MAIN_ENC_B);

    uint8_t currentEncoderState = (enc_a_state << 1) | enc_b_state;

    uint16_t volumeUp = HID_USAGE_CONSUMER_VOLUME_INCREMENT;
    uint16_t volumeDown = HID_USAGE_CONSUMER_VOLUME_DECREMENT;
    uint16_t release = 0;
    // Determine direction by comparing the current state to the previous state
    if ((global.lastEncoderState == 0b11 && currentEncoderState == 0b01) ||
        (global.lastEncoderState == 0b01 && currentEncoderState == 0b00) ||
        (global.lastEncoderState == 0b00 && currentEncoderState == 0b10) ||
        (global.lastEncoderState == 0b10 && currentEncoderState == 0b11)) {
        tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &volumeUp, 2);
	    sleep_ms(10);
	    tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &release, 2);
    } else if ((global.lastEncoderState == 0b11 && currentEncoderState == 0b10) ||
               (global.lastEncoderState == 0b10 && currentEncoderState == 0b00) ||
               (global.lastEncoderState == 0b00 && currentEncoderState == 0b01) ||
               (global.lastEncoderState == 0b01 && currentEncoderState == 0b11)) {
 
        tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &volumeDown, 2);
	    tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &release, 2);	
    }
    

    // Update the last state with the current state
    global.lastEncoderState = currentEncoderState;
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
void get_keyboard_status(uint8_t* key_report, size_t len){

    if (global.bufferFrames != 0) {
        global.bufferFrames--;
        memset(key_report, 0, len);
        return;
    }

    uint8_t keyState[TOTAL_BITS] = {0};

    int keyIndex = 2;  // Start filling regular key codes from the third byte
    bool anyKeyPressed = false;  // Flag to check if any key is pressed

    if (!is_queue_empty(&queue)) {
        key_report[keyIndex++] = dequeue(&queue);
        anyKeyPressed = true;
    }

    uint8_t *key_map= (global.rightSideConnected && !global.numpadConnected) ? key_map_noright:key_map_standard;

    uint8_t startKeyboardState = (global.rightSideConnected << 1 | global.numpadConnected);

    gpio_put(SHIFT_REG_SH_LD, 0); // Set shift register shift/~load pin low to load in data.
    sleep_us(1);              // Short delay
    gpio_put(SHIFT_REG_SH_LD, 1); // Set  shift register shift/~load pin high to prepare to shift data.
    sleep_us(1);              // Short delay
    
    //This loop shifts the data for n number of keys to read the state of each key.
    for (int i = 0; i < TOTAL_BITS; i++) {

        uint8_t currentKeyboardState = (global.rightSideConnected << 1 | global.numpadConnected);

        if (currentKeyboardState != startKeyboardState){
            memset(key_report, 0, len);
            global.bufferFrames = 8;
            break;
        }

        bool currentPressed = gpio_get(SHIFT_REG_SERIAL_OUT) ? 0 : 1; // Read the bit, flip it so 0 means not pressed, 1 means pressed.

        uint8_t key = key_map[i];
        if (global.gameMode && (key == HID_KEY_GUI_LEFT || key == HID_KEY_GUI_RIGHT)) { //if game mode is active, skip windows.
            continue;  
        }

        keyStatus[i].lastEightSamples = ((keyStatus[i].lastEightSamples << 1) | (currentPressed & 1)) & 0xFF;       
        

        
        if (currentPressed && !keyStatus[i].held && ((keyStatus[i].lastEightSamples & 0x1F) == 0)) {           //we check against the last 5 keys.          
            //new press, last five presses were 0.
            keyState[i] = true;
        } else if (currentPressed && ((keyStatus[i].held) ||((keyStatus[i].lastEightSamples & 0x1F) == 0x1F))) {   
            keyStatus[i].held = true;
            keyState[i] = true;
        } else if (!currentPressed) {
            keyStatus[i].held = false;
            keyState[i] = false;
        }
            
        if (keyState[i]){
            anyKeyPressed = true;

            if (key_is_modifier(key)) { 
                key_report[0] |= key_modifier_bit(key);
            } else {
                if (keyIndex < 8) {
                    switch (key){
                        case HID_KEY_F13:
                            key_report[0] |= key_modifier_bit(HID_KEY_GUI_LEFT);
                            key_report[keyIndex++] = HID_KEY_SPACE;
                            enqueue(&queue, HID_KEY_GRAVE);
                            break;
                        case HID_KEY_F14:
                        case HID_KEY_F15:
                        case HID_KEY_F16:
                        case HID_KEY_F17:
                        default: 
                            key_report[keyIndex++] = key;
                            if ((key != HID_KEY_SPACE) && (keyStatus[i].held == false)) {
                                global.characters_typed++;
                            }

                    }
                  
                }
            }

        }
        gpio_put(SHIFT_REG_CLK, 1);  // Pulse the clock to shift the data
        sleep_us(1);              // Short delay
        gpio_put(SHIFT_REG_CLK, 0);  // Reset clock to low
        sleep_us(1);              // Short delay
    }

    if (!anyKeyPressed) {
        memset(key_report, 0, len);
    }
    
    if (key_report[0] & key_modifier_bit(HID_KEY_CONTROL_LEFT) && key_report[2] == HID_KEY_COMMA) {
        global.gameMode = !global.gameMode;
    }

}

