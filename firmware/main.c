/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include <tusb.h>
#include "usb_descriptors.h"

#include "ssd1306.h"
#include "pico/multicore.h"
#include "hardware/i2c.h"
#include "waveanim.h"
#include "bootanim.h"

#include <math.h>


// GPIO PIN DEFINITIONS

#define OLED_SDA 0
#define OLED_SCL 1 
#define USB_BOOT 2
#define rightCheck 3
#define numpadCheck 4
#define numlockScrlockLED 5
#define capsLED 6
#define ENC_A 7
#define ENC_B 11
#define shreg_SH_LD 16
#define shreg_SerialOut 17
#define shreg_CLK 18
#define spaceBar 19
#define RTC_RST 20
#define RTC_INT 21
#define RTC_SDA 22
#define RTC_SCL 23
#define RTC_32KHZ 36

// END GPIO PIN DEFINITIONS

#define TOTAL_BITS 120 //The total number of keys on the keyboard. 8-bits x 15 shift regs = 120 bits.
#define TOTAL_KEYS 121 //120 bits for shift reg + 1 for spacebar.

#define REPORT_ID_KEYBOARD 1

#define DEBOUNCE_DELAY_MS 10           // Debounce delay to filter out bouncing
#define PRESS_AND_HOLD_DELAY_MS 250   // Time before key repeat starts
#define REPEAT_RATE_MS 50             // Delay between repeated keypresses

#define WPM_CALC_INTERVAL_MS 5000  // Interval to calculate WPM in milliseconds (e.g., 5 seconds)
#define CHARACTERS_PER_WORD 5

#define IDLE_SPEED 10  // below this wpm value your animation will idle

#define ACTIVE_SPEED 40  // above this wpm value typing animation to trigger

#define TIME_BUFFER_SIZE 8



volatile uint8_t lastEncoderState = 0;

typedef struct {
    uint32_t last_interrupt_time;
    uint32_t characters_typed;      // Counts characters typed
    uint32_t last_wpm_calc_time;    // Last time WPM was calculated
    volatile int wpm;               // Words per minute
    uint8_t currentFrame;
    bool gameMode;
} globalVariables;

globalVariables global = {0};
/*
globalVariables global = {
    .last_interrupt_time = 0,
    .characters_typed = 0,
    .last_wpm_calc_time = 0,
    .wpm = 0,
    .currentFrame = 0,
    .gameMode = false 
};
*/

static uint8_t current_hour = 12;
static uint8_t current_minute = 0; 
static bool current_isAM = true;
static absolute_time_t last_minute_update;



void hid_task(void);
void gpio_initialize();
void animation();
static void calculate_wpm(uint32_t current_time);


void update_time() {
    absolute_time_t now = get_absolute_time();

    if (absolute_time_diff_us(last_minute_update, now) >= 60000000){
        current_minute++;
        if (current_minute >= 60){
            current_minute = 0;
            current_hour++;
            if (current_hour == 12){
                current_isAM = !current_isAM;
            }
            if (current_hour > 12){
                current_hour = 1;
            }
        }
        last_minute_update = now; 
    }

}


/* OLED Animation function. */
/*Runs on the second RP2040 core (Core 1), separate from the keyboard main code.
* This function displays an animation on the OLED screen, with a WPM counter.
* The current animation consists of a wave that ripples across the screen through 
* 10 frames that loop continuously. The speed of the animation is controlled by the WPM value.
* Each frame is located in the wave array in images.h.
*/


void cdc_animation() {
    ssd1306_t disp;
    disp.external_vcc = false;
    ssd1306_init(&disp, 128, 32, 0x3C, i2c1);
    ssd1306_clear(&disp);
    char time_str[TIME_BUFFER_SIZE];
    char time_wpm_str[15] = {0};
    uint16_t delay_time = 200;
    int wpm = 0;
    uint32_t last_wpm_time = board_millis();  // Track the last time WPM was non-zero
    const uint32_t timeout_ms = 30000;  // 30 seconds of inactivity before turning off display
    
    bool display_on = true;

    bool displayWPM = false;
    bool displayTime = false;

    //startup animation.
    for (int i = 0; i < BOOTANIMFRAMES; i++) {
    	ssd1306_bmp_show_image(&disp, startup_frames[i], frameSize);
    }
    while (1) {
	if (multicore_fifo_rvalid()) {
		uint8_t header = multicore_fifo_pop_blocking();
		//New data from windows
		
		//"A" means turn on. "B" means turn off.
		//1 means WPM. So message "1A" means turn ON WPM display. Message "1B" means turn off WPM display.
		//2: display time
		//3: Time change (timezone update). Sends the current time to the keyboard in XX:XX Format.
		//4: game mode (4A -> disable win key)
		uint8_t action = multicore_fifo_pop_blocking();
		switch(header){
			case 1:
				displayWPM = (action == 'A');
				break;
			case 2:
				displayTime = (action == 'A');
				break;
			case 3:
                for (int i = 0; i < TIME_BUFFER_SIZE; i++){
                    time_str[i] = multicore_fifo_pop_blocking();
                }
                current_hour = (time_str[0] - '0') * 10 + (time_str[1] - '0');
                current_minute = (time_str[3] - '0' * 10) + (time_str[4] - '0');
                current_isAM = (time_str[5] == 'A');
				break;
			case 4:
				global.gameMode = (action == 'A');
				break;
			default:
				break;
		}	
		 
	}
        wpm = global.wpm;

        if (wpm == 0) {
            // If WPM is zero, check how long it's been zero
            if (board_millis() - last_wpm_time >= timeout_ms) {
                if (display_on) {
                    ssd1306_clear(&disp);  // Clear the display
                    ssd1306_poweroff(&disp);  // Turn off the display
                    display_on = false;
                }
            }
        } else {
            // WPM is non-zero, reset the timer and ensure display is on
            last_wpm_time = board_millis();
            if (!display_on) {
                ssd1306_poweron(&disp);  // Turn on the display
                display_on = true;
            }
        }

        if (display_on) {
            if (wpm <= 10) {
                delay_time = 200;  // Slow FPS for "idle".
            } else if (wpm <= 40 && wpm > 10) {
                delay_time = 150;  // Steadily increase for each WPM range.
            } else if (wpm <= 80 && wpm > 40) {
                delay_time = 100;  
            } else if (wpm <= 100 && wpm > 80) {
                delay_time = 75;  
            } else {
                delay_time = 50;  // Fastest animation speed while looking good.
            }

            // Update the WPM string with options:
            if (displayTime) {
                sprintf(time_wpm_str, "%d:%02d %s ", current_hour, current_minute, current_isAM ? "AM" : "PM") ;
	        }
            if (displayWPM) {
                snprintf(time_wpm_str, sizeof(time_wpm_str), "WPM:%03d ", wpm);
            } 

            if (global.gameMode) {
                snprintf(time_wpm_str, sizeof(time_wpm_str), "GAMEMODE: ON ");
            }
                // Display the current frame and WPM
                ssd1306_show_image_with_text(&disp, wave_frames[global.currentFrame], frameSize, time_wpm_str, 1, 0, 0);

                // Update the frame index
                global.currentFrame = (global.currentFrame + 1) % 10;  // Loop through 10 frames
        }

        // Delay before showing the next frame
        sleep_ms(delay_time);
        update_time();
    }
}


/*WPM CALCULATOR*/
/* Calculates the current WPM of the user by taking the number of keys typed, extrapolating to a minute
* and using an average of 5 letters per word. Its a little  inaccurate, so tweaking in the future is
*needed.
*/
static void calculate_wpm(uint32_t current_time) {
    uint32_t time_elapsed_ms = current_time - global.last_wpm_calc_time;

    if (time_elapsed_ms >= WPM_CALC_INTERVAL_MS) {
        if (global.characters_typed > 0) {
            // Calculate WPM only if new characters were typed
            float time_elapsed_min = (float)time_elapsed_ms / 60000.0;  // Convert ms to minutes
            float current_wpm = ((float)global.characters_typed / CHARACTERS_PER_WORD) / time_elapsed_min;

            // Update WPM directly with new value, no rolling average
            global.wpm = (uint32_t)current_wpm;

            // Reset characters_typed for the next interval
            global.characters_typed = 0;
        } else {
            // No new characters typed: Decay WPM towards zero
            if (global.wpm > 0) {
                // Decrease WPM over time, adjust the decay rate as needed
                global.wpm = global.wpm * 0.5; // Decays WPM by 10% each interval
            } else {
                global.wpm = 0; // Ensure WPM doesn't go negative
            }
        }

        // Update the last calculation time
        global.last_wpm_calc_time = current_time;
    }
}

//static uint8_t last_state = 0b11;  // Initialize with the idle state (11)
    
/* Called when the encoder is rotated in clockwise or counterclockwise direction. */

void encoderCallBack(uint gpio, uint32_t events) {

    static uint32_t lastValidTime = 0;
    uint32_t currentTime = board_millis();

    if ((currentTime - lastValidTime) < 20) {
        return;
    }
    lastValidTime = currentTime;


    uint8_t enc_a_state = gpio_get(ENC_A);
    uint8_t enc_b_state = gpio_get(ENC_B);

    uint8_t currentEncoderState = (enc_a_state << 1) | enc_b_state;

    uint16_t volumeUp = HID_USAGE_CONSUMER_VOLUME_INCREMENT;
    uint16_t volumeDown = HID_USAGE_CONSUMER_VOLUME_INCREMENT;
    uint16_t release = 0;
    // Determine direction by comparing the current state to the previous state
    if ((lastEncoderState == 0b11 && currentEncoderState == 0b01) ||
        (lastEncoderState == 0b01 && currentEncoderState == 0b00) ||
        (lastEncoderState == 0b00 && currentEncoderState == 0b10) ||
        (lastEncoderState == 0b10 && currentEncoderState == 0b11)) {
        tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &volumeUp, 2);
	sleep_ms(10);
	tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &release, 2);
    } else if ((lastEncoderState == 0b11 && currentEncoderState == 0b10) ||
               (lastEncoderState == 0b10 && currentEncoderState == 0b00) ||
               (lastEncoderState == 0b00 && currentEncoderState == 0b01) ||
               (lastEncoderState == 0b01 && currentEncoderState == 0b11)) {
 
        tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &volumeDown, 2);
	tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &release, 2);	
    }
    

    // Update the last state with the current state
    lastEncoderState = currentEncoderState;
}

void gpio_initialize() {

    gpio_init(USB_BOOT);
    gpio_set_dir(USB_BOOT, GPIO_IN);

    gpio_init(rightCheck);
    gpio_set_dir(rightCheck, GPIO_IN);

    gpio_init(numpadCheck);
    gpio_set_dir(numpadCheck, GPIO_IN);

    gpio_init(numlockScrlockLED);
    gpio_set_dir(numlockScrlockLED, GPIO_IN); //IN means both off. Assert for numlock on, deassert for scrlock on.

    gpio_init(capsLED);
    gpio_set_dir(capsLED, GPIO_OUT); //Assert for caps on, deassert for caps off.

    gpio_init(shreg_SH_LD);
    gpio_set_dir(shreg_SH_LD, GPIO_OUT);

    gpio_init(shreg_SerialOut);
    gpio_set_dir(shreg_SerialOut, GPIO_IN);

    gpio_init(shreg_CLK);
    gpio_set_dir(shreg_CLK, GPIO_OUT);

    gpio_init(spaceBar);
    gpio_set_dir(spaceBar, GPIO_IN);

    gpio_init(RTC_RST);
    gpio_set_dir(RTC_RST, GPIO_OUT);

    gpio_init(RTC_INT);
    gpio_set_dir(RTC_INT, GPIO_IN);

    gpio_init(RTC_SDA);
    gpio_set_dir(RTC_SDA, GPIO_IN);

    gpio_init(RTC_SCL);
    gpio_set_dir(RTC_SCL, GPIO_IN);

    gpio_init(RTC_32KHZ);
    gpio_set_dir(RTC_32KHZ, GPIO_IN);

    gpio_init(ENC_A);
    gpio_set_dir(ENC_A, GPIO_IN);
    gpio_disable_pulls(ENC_A);

    gpio_init(ENC_B);
    gpio_set_dir(ENC_B, GPIO_IN);
    gpio_disable_pulls(ENC_B);

    lastEncoderState = (gpio_get(ENC_A)<<1) | gpio_get(ENC_B);
    // Register the callback for ENC_A on falling edge
    gpio_set_irq_enabled_with_callback(ENC_A, GPIO_IRQ_EDGE_FALL, true, &encoderCallBack);
   
    i2c_init(i2c1, 400000);
    gpio_set_function(OLED_SDA, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_SDA);
    gpio_pull_up(OLED_SCL);


}
/*------------- MAIN -------------*/
int main(void)
{
  board_init();
  tusb_init();

  // init device stack on configured roothub port
  tud_init(BOARD_TUD_RHPORT);

  gpio_initialize();

  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

  multicore_launch_core1(cdc_animation);

  while (1)
  {
    tud_task(); // tinyusb device task
    hid_task();
    
  }


  return 0;
}

//----------------------------------------------------------------
//USB CDC
//-----------------------------------------------------------------



// callback when data is received on a CDC interface
void tud_cdc_rx_cb(uint8_t itf)
{
   uint8_t temp_buffer[CFG_TUD_CDC_RX_BUFSIZE];
    // read the available data 
   uint32_t count = tud_cdc_n_read(itf, temp_buffer, sizeof(temp_buffer));
   for (uint32_t i = 0; i < count; i++){
  	multicore_fifo_push_blocking(temp_buffer[i]);
   }
   //echo back ack message
   tud_cdc_n_write(itf, (uint8_t const *) "OK\r\n", 4);
   tud_cdc_n_write_flush(itf);

}


//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+
// HID_KEY_F21 is top left numpad macro.
uint8_t key_map_standard[TOTAL_KEYS] = {
    HID_KEY_5, HID_KEY_T, HID_KEY_G, HID_KEY_B, HID_KEY_V, HID_KEY_C, HID_KEY_F, HID_KEY_D,
    HID_KEY_4, HID_KEY_R, HID_KEY_E, HID_KEY_F4, HID_KEY_F3, HID_KEY_W, HID_KEY_3, HID_KEY_X,
    HID_KEY_S, HID_KEY_A, HID_KEY_Z, HID_KEY_Q, HID_KEY_ALT_LEFT, HID_KEY_CONTROL_LEFT, HID_KEY_2, HID_KEY_GUI_LEFT,
    HID_KEY_SHIFT_LEFT, HID_KEY_CAPS_LOCK, HID_KEY_TAB, HID_KEY_F2, HID_KEY_F1, HID_KEY_ESCAPE, HID_KEY_1, HID_KEY_GRAVE,
    HID_KEY_F20/*UNPLUGGED*/, HID_KEY_F20 /*UNPLUGGED*/, HID_KEY_F20 /*UNPLUGGED*/, 
    HID_KEY_F17, HID_KEY_F20, HID_KEY_F14, HID_KEY_F16, HID_KEY_F15,

    HID_KEY_PAGE_UP, HID_KEY_F20 /*UNPLUGGED*/, HID_KEY_F20 /*UNPLUGGED*/, HID_KEY_INSERT, HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,
    HID_KEY_PAGE_DOWN, HID_KEY_HOME, HID_KEY_F20 /*UNPLUGGED*/, HID_KEY_DELETE, HID_KEY_F12, HID_KEY_ARROW_RIGHT, HID_KEY_BACKSPACE, HID_KEY_ARROW_DOWN,
    HID_KEY_ARROW_LEFT, HID_KEY_ARROW_UP, HID_KEY_BACKSLASH, HID_KEY_ENTER, HID_KEY_F11, HID_KEY_SHIFT_RIGHT, HID_KEY_CONTROL_RIGHT, HID_KEY_GUI_RIGHT, 
    HID_KEY_EQUAL, HID_KEY_BRACKET_RIGHT /*close bracket*/, HID_KEY_SLASH, HID_KEY_APOSTROPHE, HID_KEY_F9, HID_KEY_MINUS, HID_KEY_BRACKET_LEFT/*open bracket*/,  HID_KEY_ALT_RIGHT,
    HID_KEY_0, HID_KEY_SEMICOLON, HID_KEY_COMMA, HID_KEY_PERIOD, HID_KEY_F9, HID_KEY_P, HID_KEY_L, HID_KEY_O,
    HID_KEY_M, HID_KEY_9, HID_KEY_K, HID_KEY_I, HID_KEY_F8, HID_KEY_J, HID_KEY_8, HID_KEY_N,
    HID_KEY_7, HID_KEY_U, HID_KEY_H, HID_KEY_F7, HID_KEY_F6, HID_KEY_F5, HID_KEY_Y, HID_KEY_6,

    HID_KEY_KEYPAD_SUBTRACT, HID_KEY_KEYPAD_ADD, HID_KEY_F20 /*UNPLUGGED*/,  HID_KEY_F24 /*VOLUMEKNOB*/, HID_KEY_KEYPAD_ENTER, HID_KEY_KEYPAD_3,
    HID_KEY_KEYPAD_6, HID_KEY_KEYPAD_9, HID_KEY_KEYPAD_DIVIDE, HID_KEY_KEYPAD_DECIMAL, HID_KEY_KEYPAD_MULTIPLY, HID_KEY_F23 /*Top right half key*/, HID_KEY_F20 /*UNPLUGGED*/,
    HID_KEY_KEYPAD_2, HID_KEY_KEYPAD_5, HID_KEY_KEYPAD_8, HID_KEY_F20 /*UNPLUGGED*/, HID_KEY_KEYPAD_0, HID_KEY_KEYPAD_1, HID_KEY_F22 /*Top middle half key*/, HID_KEY_F21 /*Top left half key*/,
    HID_KEY_KEYPAD_4, HID_KEY_KEYPAD_7, HID_KEY_NUM_LOCK,

    HID_KEY_SPACE

};

uint8_t key_map_noright[TOTAL_KEYS] = {
    HID_KEY_5, HID_KEY_T, HID_KEY_G, HID_KEY_B, HID_KEY_V, HID_KEY_C, HID_KEY_F, HID_KEY_D,
    HID_KEY_4, HID_KEY_R, HID_KEY_E, HID_KEY_F4, HID_KEY_F3, HID_KEY_W, HID_KEY_3, HID_KEY_X,
    HID_KEY_S, HID_KEY_A, HID_KEY_Z, HID_KEY_Q, HID_KEY_ALT_LEFT, HID_KEY_CONTROL_LEFT, HID_KEY_2, HID_KEY_GUI_LEFT,
    HID_KEY_SHIFT_LEFT, HID_KEY_CAPS_LOCK, HID_KEY_TAB, HID_KEY_F2, HID_KEY_F1, HID_KEY_ESCAPE, HID_KEY_1, HID_KEY_GRAVE,
    HID_KEY_F20 /*UNPLUGGED*/, HID_KEY_F20 /*UNPLUGGED*/, HID_KEY_F20 /*UNPLUGGED*/, 
    HID_KEY_F17, HID_KEY_F20, HID_KEY_F14, HID_KEY_F16, HID_KEY_F15,

    HID_KEY_KEYPAD_SUBTRACT, HID_KEY_KEYPAD_ADD, HID_KEY_F20 /*UNPLUGGED*/,  HID_KEY_F24 /*VOLUMEKNOB*/, HID_KEY_KEYPAD_ENTER, HID_KEY_KEYPAD_3,
    HID_KEY_KEYPAD_6, HID_KEY_KEYPAD_9, HID_KEY_KEYPAD_DIVIDE, HID_KEY_KEYPAD_DECIMAL, HID_KEY_KEYPAD_MULTIPLY, HID_KEY_F23 /*Top right half key*/, HID_KEY_F20 /*UNPLUGGED*/,
    HID_KEY_KEYPAD_2, HID_KEY_KEYPAD_5, HID_KEY_KEYPAD_8, HID_KEY_F20 /*UNPLUGGED*/, HID_KEY_KEYPAD_0, HID_KEY_KEYPAD_1, HID_KEY_F22 /*Top middle half key*/, HID_KEY_F21 /*Top left half key*/,
    HID_KEY_KEYPAD_4, HID_KEY_KEYPAD_7, HID_KEY_NUM_LOCK,

    HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,
    HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,
    HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,
    HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,
    HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,
    HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,
    HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,HID_KEY_F20 /*UNPLUGGED*/,

    HID_KEY_SPACE

};


void read_keys(uint8_t* keyState) {
    static uint8_t raw_state[TOTAL_BITS] = {0};        // Last raw reading
    static uint8_t lastState[TOTAL_BITS] = {0};        // Last raw reading
    static uint8_t debounced_state[TOTAL_BITS] = {0};  // Stable debounced state
    static uint32_t last_change_time[TOTAL_BITS] = {0};


    gpio_put(shreg_SH_LD, 0); // Set shift register shift/~load pin low to load in data.
    sleep_us(1);              // Short delay
    gpio_put(shreg_SH_LD, 1); // Set  shift register shift/~load pin high to prepare to shfit data.
    sleep_us(1);              // Short delay
    
    //This loop shifts the data for n number of keys to read the state of each key.
    for (int i = 0; i < TOTAL_BITS; i++) {
        gpio_put(shreg_CLK, 1);  // Pulse the clock to shift the data
        sleep_us(1);              // Short delay

        bool current_raw = gpio_get(shreg_SerialOut); // Read the bit
        uint32_t current_time = board_millis();       

        if (current_raw != raw_state[i]) {
            // Raw state changed - reset debounce timer
            raw_state[i] = current_raw;
            last_change_time[i] = current_time;
        } else if ((current_time - last_change_time[i]) >= DEBOUNCE_DELAY_MS) {
            // Raw state has been stable for debounce period
            debounced_state[i] = raw_state[i];
        }
        
        keyState[i] = debounced_state[i] ? 0 : 1;

        keyState[i] = lastState[i] ? 0 : 1; // Store the bit (invert logic: 0 means pressed)
        gpio_put(shreg_CLK, 0);  // Reset clock to low
        sleep_us(1);              // Short delay
    }
    keyState[TOTAL_KEYS] = gpio_get(spaceBar) ? 0 : 1; // Store the bit (invert logic: 0 means pressed);
    
}

static uint8_t previousKeyState[TOTAL_KEYS] = {0};

bool key_is_modifier(uint8_t key) {
    return (key >= HID_KEY_CONTROL_LEFT && key <= HID_KEY_GUI_RIGHT);
}

// Function to get the corresponding bit for a modifier key
uint8_t key_modifier_bit(uint8_t key) {
    return 1 << (key - HID_KEY_CONTROL_LEFT);
}



static void keyboardLoop() {
    if (!tud_hid_ready()) return;

    uint8_t keyState[TOTAL_KEYS] = {0};
    read_keys(keyState);

    uint8_t key_report[8] = {0};  // Key report to be sent (8 bytes)
    int key_index = 2;  // Start filling regular key codes from the third byte
    uint32_t current_time = board_millis();  // Get current time in milliseconds
    bool key_pressed = false;  // Flag to check if any key is pressed

    //check the state of the keyboard; use secondary table if rightside is disconnected, numpad is connected.
    uint8_t* key_map = (gpio_get(rightCheck) == 1 && gpio_get(numpadCheck) == 0) ? key_map_noright : key_map_standard;

    for (int i = 0; i < TOTAL_KEYS; i++) {
        if (keyState[i]) {
            uint8_t key = key_map[i];
          
            if (global.gameMode && (key == HID_KEY_GUI_LEFT)) { //if game mode is active, skip left windows key.
                continue;                                       
            }
            key_pressed = true;

            if (key_is_modifier(key)) {                         // Handle modifier keys (e.g., ALT, CTRL)
                key_report[0] |= key_modifier_bit(key);         //if key is a modifier key, add to report.
            } else {                                            // Handle regular keys
                if (!previousKeyState[i]) {                   // Key has just been pressed
                    if (key_index < 8) {
                        key_report[key_index++] = key;
                        if (key != HID_KEY_SPACE) {
                            global.characters_typed++;  // only count the initial press
                        }
                    }
                } else {
                    // Key is held down, continue sending the same report
                    if (key_index < 8) {
                        key_report[key_index++] = key;
                    }
                }
            }
        }

        previousKeyState[i] = keyState[i];  // Update the previous state
    }

    // If no key is pressed, send an empty report (all keys released)
    if (!key_pressed) {
        memset(key_report, 0, sizeof(key_report));
    }

    if (key_report[0] & key_modifier_bit(HID_KEY_CONTROL_LEFT) && key_report[2] == HID_KEY_COMMA) {
        global.gameMode = !global.gameMode;
    }

    // Send key report
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, key_report[0], &key_report[2]);

    // Optional: Handle additional functions

    calculate_wpm(current_time);
}

// Every 10ms, Send 1 report for each HID profile (keyboard, mouse etc ..)
// tud_hid_report_complete_cb() is used to send the next report after previous one is complete
void hid_task(void)
{
  // Remote wakeup
  if ( tud_suspended())
  {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    tud_remote_wakeup();
  }else
  {
    keyboardLoop();
    // Send the 1st of report chain, the rest will be sent by tud_hid_report_complete_cb()
    //send_hid_report(REPORT_ID_KEYBOARD, btn);
  }
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) instance;
  (void) len;

  uint8_t next_report_id = report[0] + 1u;

  if (next_report_id < REPORT_ID_COUNT)
  {
    keyboardLoop();
  }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  (void) instance;
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{

}


void tud_umount_cb(void)
{

}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  //blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  //blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}


