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




volatile uint8_t lastEncoderState = 0;


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


void right_check_irq (uint gpio, uint32_t events) {
    if (events & GPIO_IRQ_EDGE_RISE){
        global.rightSideConnected = false;
    }
    if (events & GPIO_IRQ_EDGE_FALL) {
        global.rightSideConnected = true;
    }
}

void numpad_check_irq (uint gpio, uint32_t events) {
    if (events & GPIO_IRQ_EDGE_RISE){
        global.numpadConnected = false;
    }
    if (events & GPIO_IRQ_EDGE_FALL) {
        global.numpadConnected = true;
    }
}


void gpio_initialize() {

    gpio_init(USB_BOOT);
    gpio_set_dir(USB_BOOT, GPIO_IN);

    gpio_init(RIGHT_CHECK);
    gpio_set_dir(RIGHT_CHECK, GPIO_IN);
    gpio_set_irq_enabled_with_callback(
        RIGHT_CHECK,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
        true,
        &right_check_irq
    );

    gpio_init(NUMPAD_CHECK);
    gpio_set_dir(NUMPAD_CHECK, GPIO_IN);
        gpio_set_irq_enabled_with_callback(
        NUMPAD_CHECK,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
        true,
        &numpad_check_irq
    );

    gpio_init(NUM_LOCK_SCROLL_LOCK_LED);
    gpio_set_dir(NUM_LOCK_SCROLL_LOCK_LED, GPIO_IN); //IN means both off. Assert for numlock on, deassert for scrlock on.

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

    gpio_init(NUM_ENC_B_I2C_SCL);
    gpio_set_dir(NUM_ENC_B_I2C_SCL, GPIO_IN);


    lastEncoderState = (gpio_get(MAIN_ENC_A)<<1) | gpio_get(MAIN_ENC_B);
    // Register the callback for ENC_A on falling edge
    gpio_set_irq_enabled_with_callback(MAIN_ENC_A, GPIO_IRQ_EDGE_FALL, true, &encoderCallBack);
   
    i2c_init(i2c1, 400000);
    gpio_set_function(OLED_SDA, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_SDA);
    gpio_pull_up(OLED_SCL);


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
    if (!tud_hid_ready()) return;

    uint8_t key_report[8] = {0};
    get_keyboard_status(key_report, 8);

    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, key_report[0], &key_report[2]);
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
    uint8_t key_report[8] = {0};
    get_keyboard_status(key_report, 8);
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, key_report[0], &key_report[2]);
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

        if (caps_on) {
            gpio_put(CAPS_LOCK_LED, 1); // Turn on Caps Lock LED
        } else {
            gpio_put(CAPS_LOCK_LED, 0); // Turn off Caps Lock LED
        }

        if (!num_on && scroll_on) {
            gpio_set_dir(NUM_LOCK_SCROLL_LOCK_LED, GPIO_OUT);
            gpio_put(NUM_LOCK_SCROLL_LOCK_LED, 1); // Turn on Num Lock LED
        } else if (num_on && !scroll_on) {
            gpio_set_dir(NUM_LOCK_SCROLL_LOCK_LED, GPIO_OUT);
            gpio_put(NUM_LOCK_SCROLL_LOCK_LED, 0); // Turn on Scroll Lock LED
        } else if (num_on && scroll_on) {
          gpio_set_dir(NUM_LOCK_SCROLL_LOCK_LED, GPIO_OUT);
          global.scrollNumPWM = true;
        } else {
          global.scrollNumPWM = false;
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


/*------------- MAIN -------------*/
int main(void)
{
  board_init();
  tusb_init();

  // init device stack on configured roothub port
  //tud_init(BOARD_TUD_RHPORT);

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


