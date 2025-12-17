#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_

#include "pico/stdlib.h"
#include "config.h"
#include "usb_descriptors.h"
#include <tusb.h>

typedef struct {
    uint8_t items[QUEUE_SIZE];
    int start;
    int end;
    int count;
    uint32_t enqueue_cycle[QUEUE_SIZE];
} queue_t;


extern queue_t mainEncoderQueue;

bool get_keyboard_status(uint8_t* key_report, size_t len);

void enqueue (queue_t* q, uint8_t input);
int dequeue(queue_t* q);

#endif
