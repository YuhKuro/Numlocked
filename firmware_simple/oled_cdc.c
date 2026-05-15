#include "config.h"
#include "oled_cdc.h"
#include "waveanim.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "ssd1306.h"
#include "pico/time.h"
#include "pico/multicore.h"
#include <stdio.h>
#include <string.h>
#include "tusb_config.h"
#include "bootanim.h"
#include "tusb.h"

#include "bsp/board_api.h"

#include "walking_animation.h"
#include "idle_animation.h"
#include "asleep_animation.h"
#include "run_animation.h"


//static uint8_t current_hour = 12;
//static uint8_t current_minute = 0; 
//static bool current_isAM = true;
//static absolute_time_t last_minute_update;


void hid_task(void);
void gpio_initialize();
void animation();
static void calculate_wpm(uint32_t current_time);



//----------------------------------------------------------------
//USB CDC
//-----------------------------------------------------------------


/*
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

*/


/*WPM CALCULATOR*/
/* Calculates the current WPM of the user by taking the number of keys typed, extrapolating to a minute
* and using an average of 5 letters per word. Its a little  inaccurate, so tweaking in the future is
*needed.
*/


typedef struct {
    const unsigned char** frames;
    int frame_count;
    int delay_time;
} animation_t;


animation_t asleep_anim = {
    .frames = asleep_frames,
    .frame_count = ASLEEP_FRAME_COUNT,  // 12
    .delay_time = 250
};



animation_t idle_anim = {
    .frames = idle_frames,
    .frame_count = IDLE_FRAME_COUNT,  // 12
    .delay_time = 100
};


animation_t walking_anim = {
    .frames = walking_frames,
    .frame_count = WALKING_FRAME_COUNT,  // 6
    .delay_time = 30
};

animation_t running_anim = {
    .frames = run_frames,
    .frame_count = RUN_FRAME_COUNT,  // 4
    .delay_time = 40

};



/* OLED Animation function. */
/*Runs on the second RP2040 core (Core 1), separate from the keyboard main code.
* This function displays an animation on the OLED screen, with a WPM counter.
* The current animation consists of a wave that ripples across the screen through 
* 10 frames that loop continuously. The speed of the animation is controlled by the WPM value.
* Each frame is located in the wave array in images.h.
*/


void cdc_animation() {

    uint32_t token = multicore_fifo_pop_blocking();
    if (token != 0xBEEF) {
        // If wrong token, stop here (optional safety)
        while (1) { tight_loop_contents(); }
    }
    

    /*
    gpio_init(NUM_LOCK_SCROLL_LOCK_LED);
    gpio_set_dir(NUM_LOCK_SCROLL_LOCK_LED, GPIO_OUT);
    gpio_put(NUM_LOCK_SCROLL_LOCK_LED, 1);
    sleep_ms(100);
    gpio_put(NUM_LOCK_SCROLL_LOCK_LED, 0);
    */

    //Initialze gpio:
    i2c_init(i2c1, 400000);
    gpio_set_function(OLED_SDA, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_SDA);
    gpio_pull_up(OLED_SCL);

    sleep_ms(100);

    static ssd1306_t disp;
    disp.external_vcc = false;
    ssd1306_init(&disp, 128, 32, 0x3C, i2c1);

    sleep_ms(50);
    
    ssd1306_poweron(&disp);
    ssd1306_contrast(&disp, 255);
    
    ssd1306_clear(&disp);

    char time_wpm_str[30] = {0};
    int wpm = 0;

    uint32_t last_nonzero_time = board_millis();  // Track the last time WPM was non-zero
    const uint32_t shutoff_ms = 1800000;  // 30 minutes of inactivity before turning off display
    const uint32_t unmount_shutoff_ms = 120000;  // 2 minutes of unmounted/suspended before turning off display
    bool display_on = true;
    animation_t* current_anim = &idle_anim;
    animation_t* new_anim = NULL;
    //startup animation.
    
    //uint32_t last_heartbeat_ms = to_ms_since_boot(get_absolute_time());
    
    ssd1306_draw_string(&disp, 0, 10, 1, "Numlocked Keyboard v1.0");
    ssd1306_show(&disp);
    sleep_ms(3000);
    ssd1306_clear(&disp);


    while (1) {
        /*
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_heartbeat_ms >= 500) {
            last_heartbeat_ms = now;
            //toggle small heartbeat LED (caps LED used here)
            gpio_put(NUM_LOCK_SCROLL_LOCK_LED, !gpio_get(NUM_LOCK_SCROLL_LOCK_LED));

        }
            */
        wpm = global.wpm;
        
        if (wpm == 0) {
            // If WPM is zero, check how long it's been zero
            if ((board_millis() - last_nonzero_time >= shutoff_ms) || 
                (global.usb_mounted == false && (board_millis() - last_nonzero_time >= unmount_shutoff_ms)) || 
                ((global.usb_suspended == true) && (board_millis() - last_nonzero_time >= unmount_shutoff_ms))) {
                if (display_on) {
                    ssd1306_clear(&disp);  // Clear the display
                    ssd1306_poweroff(&disp);  // Turn off the display
                    display_on = false;
                }
            }
        } else {
            // WPM is non-zero, reset the timer and ensure display is on
            last_nonzero_time = board_millis();
            if (!display_on) {
                ssd1306_poweron(&disp);  // Turn on the display
                display_on = true;
                ssd1306_draw_string(&disp, 0, 10, 1, "Numlocked Keyboard v1.0");
                ssd1306_show(&disp);
                sleep_ms(3000);
                ssd1306_clear(&disp);
            }
        }
        
        
        if (display_on) {
            if (global.numpadOLEDChange){
                global.numpadOLEDChange = false;

                if (!display_on) {
                ssd1306_poweron(&disp);  // Turn on the display
                display_on = true;
                }
                //show numpad connection changed
                ssd1306_clear(&disp);
                if (global.numpadConnected) {
                    ssd1306_draw_string(&disp, 0, 10, 1, "Numpad Connected");
                } else {
                    ssd1306_draw_string(&disp, 0, 10, 1, "Numpad Disconnected");
                }
                ssd1306_show(&disp);
                sleep_ms(2000); // Show message for 2 seconds
                
            } else if (global.rightSideOLEDChange){
                global.rightSideOLEDChange = false;
                if (!display_on) {
                    ssd1306_poweron(&disp);  // Turn on the display
                    display_on = true;
                }
                //show right side connection changed
                ssd1306_clear(&disp);
                if (global.rightSideConnected) {
                    ssd1306_draw_string(&disp, 0, 10, 1, "Right Side Connected");
                } else {
                    ssd1306_draw_string(&disp, 0, 10, 1, "Right Side Disconnected");
                }
                ssd1306_show(&disp);
                sleep_ms(2000); // Show message for 2 seconds

                
            } else {
                    //standard animiation
                const uint32_t idleThreshold = 300000; // 5 minutes
                if (((wpm == 0) && (board_millis() - last_nonzero_time >= idleThreshold)) || (global.usb_mounted == false) || (global.usb_suspended)) {
                    new_anim = &asleep_anim;    //kirby SLEEPING
                } else if (wpm == 0) {
                    new_anim = &idle_anim;  // Kirby Standing Still
                } else if (wpm <= 70 && wpm > 0) {
                    new_anim = &walking_anim;  // Kirby Walking
                } else if (wpm <= 100 && wpm > 70) {
                    new_anim = &running_anim;  // Kirby Running
                } else {
                    new_anim = &running_anim;  // Kirby Running
                }

                if (new_anim != current_anim) {
                    current_anim = new_anim;
                    oled.currentFrame = 0;  // Reset to first frame of new animation
                }

                // Update the WPM string
                if (global.usb_mounted == false) {
                    snprintf(time_wpm_str, sizeof(time_wpm_str), "USB:UNMOUNTED");
                } else if (global.usb_suspended) {
                    snprintf(time_wpm_str, sizeof(time_wpm_str), "USB:SUSPENDED");
                } else if (global.gameMode) {
                    snprintf(time_wpm_str, sizeof(time_wpm_str), "GAMEMODE: ON");
                } else {
                    //snprintf(time_wpm_str, sizeof(time_wpm_str), "T%d", global.characters_typed);
                    snprintf(time_wpm_str, sizeof(time_wpm_str), "WPM:%03d", wpm);
                    if (global.rightSideConnected) {
                        strncat(time_wpm_str, "     R:OK", sizeof(time_wpm_str) - strlen(time_wpm_str) - 1);
                    } else {
                        strncat(time_wpm_str, "     R:--", sizeof(time_wpm_str) - strlen(time_wpm_str) - 1);
                    }
                    if (global.numpadConnected) {
                        strncat(time_wpm_str, " N:OK", sizeof(time_wpm_str) - strlen(time_wpm_str) - 1);
                    } else {
                        strncat(time_wpm_str, " N:--", sizeof(time_wpm_str) - strlen(time_wpm_str) - 1);
                    }
                }
                // Display the current frame and WPM
                ssd1306_show_image_with_text(&disp, current_anim->frames[oled.currentFrame], FRAME_SIZE, time_wpm_str, 1, 0, 0);

                // Update the frame index
                oled.currentFrame = (oled.currentFrame + 1) % current_anim->frame_count;

            }
        }

        // Delay before showing the next frame
        sleep_ms(current_anim->delay_time);
    }
}

