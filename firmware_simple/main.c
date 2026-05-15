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
#include "pico/time.h"

#include "oled_cdc.h"
#include "keyboard.h"

#include <math.h>
#include "config.h"


globalVariables global = {0};

keyboardVariables keyboard = {0};

oledVariables oled = {0};

static bool remoteWakeUpSent = false;


volatile bool send_next_report_flag = true;
volatile uint8_t last_sent_report_id = REPORT_ID_KEYBOARD;

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



static inline uint32_t ms_since(absolute_time_t t) {
  return to_ms_since_boot(get_absolute_time()) - to_ms_since_boot(t);
}




void gpio_irq_handler(uint gpio, uint32_t events) {
    static absolute_time_t last_sw_time;
    
    if (gpio == RIGHT_CHECK) {
        global.rightSideConnected = (events & GPIO_IRQ_EDGE_FALL) ? true : false;
        global.rightSideOLEDChange = true;
        keyboard.keyboardStateChanged = true;
    } else if (gpio == NUMPAD_CHECK) {
        global.numpadConnected = (events & GPIO_IRQ_EDGE_FALL) ? true : false;
        global.numpadOLEDChange = true;
        keyboard.keyboardStateChanged = true;
    } else if (gpio == MAIN_ENC_A || gpio == MAIN_ENC_B) {
        // Just set a flag - process in main loop
        keyboard.mainEncoderChanged = true;
    } else if (gpio == NUM_ENC_A_I2C_SDA || gpio == NUM_ENC_B_I2C_SCL){
        keyboard.numEncoderChanged = true;
    } else if (gpio == MAIN_ENC_SW) {
        absolute_time_t now = get_absolute_time();
        if (absolute_time_diff_us(last_sw_time, now) > ENC_SW_DEBOUNCE_MS * 1000) {
            keyboard.muteFlag = true;
            last_sw_time = now;
        }
    }
}


void process_main_encoder() {
    static uint8_t last_state = 0;
    static absolute_time_t last_change_time = 0;
    const uint32_t DEBOUNCE_US = 1000; // 1ms debounce
    if (!keyboard.mainEncoderChanged) return;
    keyboard.mainEncoderChanged = false;
    absolute_time_t now = get_absolute_time();
    if (absolute_time_diff_us(last_change_time, now) < DEBOUNCE_US) {
        return; // Ignore if too soon
    }
    uint8_t state = (gpio_get(MAIN_ENC_A) << 1) | gpio_get(MAIN_ENC_B);
    // Only process on valid transitions
    if (last_state != state) {
        if ((last_state == 0b00 && state == 0b01) ||
                    (last_state == 0b01 && state == 0b11) ||
                    (last_state == 0b11 && state == 0b10) ||
                    (last_state == 0b10 && state == 0b00)) {
                    // Clockwise
                    keyboard.mainEncoderDelta--;
                } else if ((last_state == 0b00 && state == 0b10) ||
                        (last_state == 0b10 && state == 0b11) ||
                        (last_state == 0b11 && state == 0b01) ||
                        (last_state == 0b01 && state == 0b00)) {
                    // Counter-clockwise
                    keyboard.mainEncoderDelta++;
                }
                // Else: invalid transition, ignore
                last_state = state;
                last_change_time = now;
    }
    // Clamp delta to prevent runaway
    if (keyboard.mainEncoderDelta > ENCODER_STEPS_PER_INDENT * 3) {
            keyboard.mainEncoderDelta = ENCODER_STEPS_PER_INDENT * 3;
    } else if (keyboard.mainEncoderDelta < -ENCODER_STEPS_PER_INDENT * 3) {
        keyboard.mainEncoderDelta = -ENCODER_STEPS_PER_INDENT * 3;
    }
}

void process_num_encoder(){
    static uint8_t last_state = 0;
    static absolute_time_t last_change_time = 0;
    const uint32_t DEBOUNCE_US = 1000; // 1ms debounce
    if (!keyboard.numEncoderChanged) return;
    keyboard.numEncoderChanged = false;
    absolute_time_t now = get_absolute_time();
    if (absolute_time_diff_us(last_change_time, now) < DEBOUNCE_US) {
        return; // Ignore if too soon
    }
    uint8_t state = (gpio_get(NUM_ENC_A_I2C_SDA) << 1) | gpio_get(NUM_ENC_B_I2C_SCL);
    // Only process on valid transitions
    if (last_state != state) {
        if ((last_state == 0b00 && state == 0b01) ||
            (last_state == 0b01 && state == 0b11) ||
            (last_state == 0b11 && state == 0b10) ||
            (last_state == 0b10 && state == 0b00)) {
            // Clockwise
            keyboard.numEncoderDelta--;
        } else if ((last_state == 0b00 && state == 0b10) ||
                (last_state == 0b10 && state == 0b11) ||
                (last_state == 0b11 && state == 0b01) ||
                (last_state == 0b01 && state == 0b00)) {
                // Counter-clockwise
                keyboard.numEncoderDelta++;
        }
        // Else: invalid transition, ignore
        last_state = state;
        last_change_time = now;
    }
    // Clamp delta to prevent runaway
    if (keyboard.numEncoderDelta > ENCODER_STEPS_PER_INDENT * 3) {
        keyboard.numEncoderDelta = ENCODER_STEPS_PER_INDENT * 3;
    } else if (keyboard.numEncoderDelta < -ENCODER_STEPS_PER_INDENT * 3) {
        keyboard.numEncoderDelta = -ENCODER_STEPS_PER_INDENT * 3;
    }
}

void gpio_initialize() {

  gpio_init(USB_BOOT);
  gpio_set_dir(USB_BOOT, GPIO_IN);

  gpio_init(RIGHT_CHECK);
  gpio_set_dir(RIGHT_CHECK, GPIO_IN);
  gpio_disable_pulls(RIGHT_CHECK);
  gpio_set_irq_enabled_with_callback(
      RIGHT_CHECK,  
      GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
      true,
      &gpio_irq_handler  
  );

  gpio_init(NUMPAD_CHECK);
  gpio_set_dir(NUMPAD_CHECK, GPIO_IN);
  gpio_disable_pulls(NUMPAD_CHECK);


  global.rightSideConnected = !gpio_get(RIGHT_CHECK);
  global.numpadConnected = !gpio_get(NUMPAD_CHECK); 
  
  gpio_init(NUM_LOCK_SCROLL_LOCK_LED);
  gpio_set_dir(NUM_LOCK_SCROLL_LOCK_LED, GPIO_OUT);
  gpio_set_dir(NUM_LOCK_SCROLL_LOCK_LED, GPIO_IN); //IN means both off. Assert for numlock on, deassert for scrlock on.
  gpio_disable_pulls(NUM_LOCK_SCROLL_LOCK_LED);
  
  gpio_init(CAPS_LOCK_LED);
  gpio_set_dir(CAPS_LOCK_LED, GPIO_OUT); //Assert for caps on, deassert for caps off.
  gpio_put(CAPS_LOCK_LED, 0); //start with LED off.

  gpio_init(SHIFT_REG_SH_LD);
  gpio_set_dir(SHIFT_REG_SH_LD, GPIO_OUT);

  gpio_init(SHIFT_REG_SERIAL_OUT);
  gpio_set_dir(SHIFT_REG_SERIAL_OUT, GPIO_IN);

  gpio_init(SHIFT_REG_CLK);
  gpio_set_dir(SHIFT_REG_CLK, GPIO_OUT);


  gpio_init(MAIN_ENC_A);
  gpio_set_dir(MAIN_ENC_A, GPIO_IN);
  //gpio_disable_pulls(MAIN_ENC_A);

  gpio_init(MAIN_ENC_B);
  gpio_set_dir(MAIN_ENC_B, GPIO_IN);
  //gpio_disable_pulls(ENC_B);

  gpio_init(MAIN_ENC_SW);
  gpio_set_dir(MAIN_ENC_SW, GPIO_IN);

  //initialize these as simple inputs. If Pull ups are DETECTED when the numpad is connected, then swap to dealing with them as I2C.
  gpio_init(NUM_ENC_A_I2C_SDA);
  gpio_set_dir(NUM_ENC_A_I2C_SDA, GPIO_IN);
  gpio_disable_pulls(NUM_ENC_A_I2C_SDA);

  gpio_init(NUM_ENC_B_I2C_SCL);
  gpio_set_dir(NUM_ENC_B_I2C_SCL, GPIO_IN);
  gpio_disable_pulls(NUM_ENC_B_I2C_SCL);


  keyboard.lastMainEncoderState = (gpio_get(MAIN_ENC_A)<<1) | gpio_get(MAIN_ENC_B);
  // Register the callback for ENC_A on falling edge
  
  gpio_set_irq_enabled(NUMPAD_CHECK, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
  gpio_set_irq_enabled(MAIN_ENC_A, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
  gpio_set_irq_enabled(MAIN_ENC_B, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
  gpio_set_irq_enabled(MAIN_ENC_SW, GPIO_IRQ_EDGE_FALL, true);

  gpio_set_irq_enabled(NUM_ENC_A_I2C_SDA, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
  gpio_set_irq_enabled(NUM_ENC_B_I2C_SCL, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
  
}

typedef enum {
    CONSUMER_STATE_IDLE,
    CONSUMER_STATE_PRESS_SENT,
    CONSUMER_STATE_RELEASE_PENDING
} consumer_state_t;


static consumer_state_t consumer_state = CONSUMER_STATE_IDLE;




static void send_hid_report(uint8_t report_id) {
    static absolute_time_t last_pause_time;
    if (!tud_hid_ready()) return;
    
    switch (report_id) {
        case REPORT_ID_KEYBOARD: {
            uint8_t key_report[8] = {0};
            (void)get_keyboard_status(key_report, 8);
            tud_hid_keyboard_report(REPORT_ID_KEYBOARD, key_report[0], &key_report[2]);
            break;
        }
        
        case REPORT_ID_CONSUMER_CONTROL: {
            static bool has_consumer_key = false;
            static absolute_time_t last_consumer_report_time = 0;
            absolute_time_t now = get_absolute_time();
            const uint32_t MIN_REPORT_INTERVAL_US = 20000; // 20ms minimum between reports
            
            // Check for volume control
            if (keyboard.mainEncoderDelta >= ENCODER_STEPS_PER_INDENT) {
                if (absolute_time_diff_us(last_consumer_report_time, now) > MIN_REPORT_INTERVAL_US) {
                    uint16_t volume_usage = HID_USAGE_CONSUMER_VOLUME_INCREMENT; 
                    tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &volume_usage, 2);
                    keyboard.mainEncoderDelta -= ENCODER_STEPS_PER_INDENT; // Subtract, don't zero
                    has_consumer_key = true;
                    last_consumer_report_time = now;
                }
            } else if (keyboard.mainEncoderDelta <= -ENCODER_STEPS_PER_INDENT) {
                if (absolute_time_diff_us(last_consumer_report_time, now) > MIN_REPORT_INTERVAL_US) {
                    uint16_t volume_usage = HID_USAGE_CONSUMER_VOLUME_DECREMENT;
                    tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &volume_usage, 2);
                    keyboard.mainEncoderDelta += ENCODER_STEPS_PER_INDENT; // Add, don't zero
                    has_consumer_key = true;
                    last_consumer_report_time = now;
                }
            } else if (keyboard.pauseFlag == true) {
                if (absolute_time_diff_us(last_pause_time, now) > ENC_SW_DEBOUNCE_MS * 1000) {
                    uint16_t pause_usage = HID_USAGE_CONSUMER_PLAY_PAUSE;
                    tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &pause_usage, 2);
                    has_consumer_key = true;
                    keyboard.pauseFlag = false;
                    last_pause_time = now;
                    last_consumer_report_time = now;
                }
            } else if (keyboard.muteFlag == true) {
                uint16_t mute_usage = HID_USAGE_CONSUMER_MUTE;
                tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &mute_usage, 2);
                has_consumer_key = true;
                keyboard.muteFlag = false;
                last_consumer_report_time = now;   
            } else {
                if (has_consumer_key) {
                  if (absolute_time_diff_us(last_consumer_report_time, now) > 10000) {
                    uint16_t empty_key = 0;
                    tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &empty_key, 2);
                    last_consumer_report_time = now;
                    has_consumer_key = false;
                  }
                }
            }
            break;
        }
        
        default:
            break;
    }
}
// Every Send 1ms, report for each HID profile (keyboard, mouse etc ..)
// tud_hid_report_complete_cb() is used to send the next report after previous one is complete
void hid_task(void)
{
    if (!tud_hid_ready()) return;
    
    if (!tud_suspended()) {
        global.usb_suspended = false;
        global.usb_mounted = true; 
        tud_remote_wakeup();
    }
    
    // Send keyboard report
    send_hid_report(REPORT_ID_KEYBOARD);

}

void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
    (void) instance;
    (void) len;
    
    uint8_t next_report = report[0]; // Current report ID
    
    if (next_report == REPORT_ID_KEYBOARD) {
        // After keyboard, send consumer
        send_hid_report(REPORT_ID_CONSUMER_CONTROL);
    }
    // If there are more report IDs, continue the chain here
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
void tud_hid_set_report_cb(uint8_t instance, 
                           uint8_t report_id, 
                           hid_report_type_t report_type, 
                           uint8_t const* buffer, 
                           uint16_t bufsize)
{
    (void) instance;
    (void) report_id;

    if (report_type == HID_REPORT_TYPE_OUTPUT && bufsize >= 1) {
        uint8_t led_state = buffer[0];

        bool caps_on   = led_state & KEYBOARD_LED_CAPSLOCK;
        bool num_on    = led_state & KEYBOARD_LED_NUMLOCK;
        bool scroll_on = led_state & KEYBOARD_LED_SCROLLLOCK;

        if (global.gameMode == true || !global.usb_mounted) { // If in game mode, force all LEDs off.
          gpio_put(CAPS_LOCK_LED, 0); // Turn off Caps Lock LED
          keyboard.scrollNumPWM = false;
          gpio_set_dir(NUM_LOCK_SCROLL_LOCK_LED, GPIO_IN); // Turn off both LEDs.
          return; 
        }

        if (caps_on) {
            gpio_put(CAPS_LOCK_LED, 1); // Turn on Caps Lock LED
        } else {
            gpio_put(CAPS_LOCK_LED, 0); // Turn off Caps Lock LED
        }
        
        if (!num_on && !scroll_on) {              //turn on NUMLOCK LED if Numlock is OFF and scroll lock is OFF
          gpio_set_dir(NUM_LOCK_SCROLL_LOCK_LED, GPIO_OUT);
          keyboard.scrollNumPWM = false;
          gpio_put(NUM_LOCK_SCROLL_LOCK_LED, 1);    //Turn on second (numlock) LED
        } else if (num_on && scroll_on) {         //turn on Scroll-lock LED if numlock is ON and scroll lock is ON
          gpio_set_dir(NUM_LOCK_SCROLL_LOCK_LED, GPIO_OUT);
          keyboard.scrollNumPWM = false;
          gpio_put(NUM_LOCK_SCROLL_LOCK_LED, 0); //turn on 1st (scrolllock) LED
          
        } else if (!num_on && scroll_on) {       //both LEDs ON if numlock is ON and scroll lock is ON
          gpio_set_dir(NUM_LOCK_SCROLL_LOCK_LED, GPIO_OUT);
          keyboard.scrollNumPWM = true;
        } else {                      
          keyboard.scrollNumPWM = false;
          gpio_set_dir(NUM_LOCK_SCROLL_LOCK_LED, GPIO_IN); // Turn off both LEDs.
        }

    }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  global.usb_mounted = true;
}


void tud_umount_cb(void)
{
  global.usb_mounted = false;
  keyboard.scrollNumPWM = false;
  gpio_set_dir(NUM_LOCK_SCROLL_LOCK_LED, GPIO_IN); // Turn off both LEDs.
  
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  global.usb_suspended = true;
  keyboard.scrollNumPWM = false;
  gpio_set_dir(NUM_LOCK_SCROLL_LOCK_LED, GPIO_IN); // Turn off both LEDs.
}
// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  global.usb_suspended = false;
  consumer_state = CONSUMER_STATE_IDLE; 
  keyboard.pauseFlag = false;

  remoteWakeUpSent = false;
  //blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;

  if (tud_hid_ready()) {
    uint8_t empty[8] = {0};
    tud_hid_report(REPORT_ID_KEYBOARD, empty, sizeof(empty));
  }
}

volatile bool core1_ready = false;
volatile bool core1_running = false;


void cdc_animation_wrapper(void) {
    // Signal that core1 started successfully
    multicore_fifo_push_blocking(0xC0DE1);
    core1_ready = true;
    core1_running = true;
    
    // Call your actual function
    cdc_animation();
    
    // If cdc_animation ever returns, mark as not running
    core1_running = false;
}


bool launch_core1_with_retry(int max_retries) {
    for (int i = 0; i < max_retries; i++) {
        //printf("Attempting to launch core1 (attempt %d/%d)...\n", i + 1, max_retries);
        
        // Reset flags
        core1_ready = false;
        core1_running = false;
        
        // Clear any stale FIFO data
        while (multicore_fifo_rvalid()) {
            multicore_fifo_pop_blocking();
        }
        
        // Launch core1
        multicore_launch_core1(cdc_animation_wrapper);
        
        // Wait for handshake with timeout
        absolute_time_t timeout = make_timeout_time_ms(200);  // Increased timeout
        while (!core1_ready && !time_reached(timeout)) {
            tight_loop_contents();
        }
        
        if (core1_ready) {
            //printf("Core1 launched successfully on attempt %d\n", i + 1);
            
            // Wait a bit more to ensure core1 is stable
            sleep_ms(10);
            
            // Verify core1 is still running by checking FIFO communication
            uint32_t response;
            if (multicore_fifo_pop_timeout_us(100000, &response)) {  // 100ms timeout
                if (response == 0xC0DE1) {
                    // Send your original message
                    multicore_fifo_push_blocking(0xBEEF);
                    return true;  // Success!
                }
            }
        }
        
        //printf("Core1 launch attempt %d failed, retrying...\n", i + 1);
        
        // Reset core1 before retry (if not last attempt)
        if (i < max_retries - 1) {
            multicore_reset_core1();
            sleep_ms(100);  // Give time for reset
        }
    }
    
    //printf("ERROR: Failed to launch core1 after %d attempts\n", max_retries);
    return false;
}

bool is_core1_healthy(void) {
    return core1_running && core1_ready;
}

/*------------- MAIN -------------*/
int main(void)
{
  board_init();
  sleep_ms(100);
  tusb_init();

  // init device stack on configured roothub port
  //tud_init(BOARD_TUD_RHPORT);

  gpio_initialize();



  if (board_init_after_tusb) {
    board_init_after_tusb();
  }
 const int MAX_CORE1_RETRIES = 3;
    if (!launch_core1_with_retry(MAX_CORE1_RETRIES)) {
        // Handle failure - you might want to continue without core1 or halt
        //printf("CRITICAL: Could not start core1, continuing without animation\n");
        // You could set a flag here to disable features that depend on core1
    }

  //multicore_launch_core1(cdc_animation);

  //multicore_fifo_push_blocking(0xBEEF);

  uint32_t lastPWMTime = to_ms_since_boot(get_absolute_time());
  //uint32_t last_heartbeat_ms = to_ms_since_boot(get_absolute_time());

  keyboard.keyQueueDelay = 15; 
  while (1)
  {
    tud_task(); // tinyusb device task
    hid_task(); 
    process_main_encoder();
    process_num_encoder();
    uint32_t now = to_ms_since_boot(get_absolute_time());
    /*
    if (now - last_heartbeat_ms >= 500) {
      last_heartbeat_ms = now;
       //toggle small heartbeat LED (caps LED used here)
      gpio_put(CAPS_LOCK_LED, !gpio_get(CAPS_LOCK_LED));

    }
    */
    if (now - lastPWMTime >= 2) {
      //last_heartbeat_ms = now;
      // toggle small heartbeat LED (caps LED used here)
      //gpio_put(CAPS_LOCK_LED, !gpio_get(CAPS_LOCK_LED));

      // handle scroll/num LED toggling only here (non-blocking)
      if (keyboard.scrollNumPWM) {
        gpio_put(NUM_LOCK_SCROLL_LOCK_LED, !gpio_get(NUM_LOCK_SCROLL_LOCK_LED));
        lastPWMTime = now;
      }
    }
    
    
  }


  return 0;
}


