#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_

#include "pico/stdlib.h"
#include "config.h"
#include "usb_descriptors.h"
#include <tusb.h>




void encoderCallBack(uint gpio, uint32_t events);

void get_keyboard_status(uint8_t* key_report, size_t len);

#endif
