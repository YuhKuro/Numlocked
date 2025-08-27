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



static uint8_t current_hour = 12;
static uint8_t current_minute = 0; 
static bool current_isAM = true;
static absolute_time_t last_minute_update;


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


*/

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
    
    uint32_t last_wpm_time = board_millis();  // Track the last time WPM was non-zero
    const uint32_t timeout_ms = 30000;  // 30 seconds of inactivity before turning off display
    
    bool display_on = true;

    bool displayWPM = false;
    bool displayTime = false;

    //startup animation.
    for (int i = 0; i < BOOTANIMFRAMES; i++) {
    	ssd1306_bmp_show_image(&disp, startup_frames[i], startUpFrameSize);
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
        int wpm = global.wpm;

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
            } else if (wpm <= 40) {
                delay_time = 150;  // Steadily increase for each WPM range.
            } else if (wpm <= 80) {
                delay_time = 100;  
            } else if (wpm <= 100) {
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
                ssd1306_show_image_with_text(&disp, wave_frames[global.currentFrame], waveFrameSize, time_wpm_str, 1, 0, 0);

                // Update the frame index
                global.currentFrame = (global.currentFrame + 1) % 10;  // Loop through 10 frames
        }

        // Delay before showing the next frame
        sleep_ms(delay_time);
        calculate_wpm(board_millis());
        update_time();
    }
}


