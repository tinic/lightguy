/*
Copyright 2019 Tinic Uro

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include <stdio.h>

extern "C" {
#include "gd32f10x.h"
}

#include "./main.h"
#include "./status.h"
#include "./systick.h"

namespace lightkraken {

StatusLED &StatusLED::instance() {
    static StatusLED status_led;
    if (!status_led.initialized) {
        status_led.initialized = true;
        status_led.init();
    }
    return status_led;
}

void StatusLED::init() {
    rcu_periph_clock_enable(RCU_GPIOB);

    gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_6);    
    gpio_bit_reset(GPIOB, GPIO_PIN_6);

#ifndef BOOTLOADER
    gpio_init(GPIOA, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, GPIO_PIN_3);
    gpio_init(GPIOA, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, GPIO_PIN_5);
    gpio_init(GPIOA, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, GPIO_PIN_4);
    gpio_init(GPIOB, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, GPIO_PIN_0);
#endif  // #ifndef BOOTLOADER

}

#ifndef BOOTLOADER
void StatusLED::readPowerState() {
    bt_state = gpio_input_bit_get(GPIOA, GPIO_PIN_3) == RESET ? false : true;
    tpl_state = gpio_input_bit_get(GPIOA, GPIO_PIN_5) == RESET ? false : true;
    tph_state = gpio_input_bit_get(GPIOA, GPIO_PIN_4) == RESET ? false : true;
    powergood_state = gpio_input_bit_get(GPIOB, GPIO_PIN_0) == RESET ? false : true;
}
#endif  // #ifndef BOOTLOADER

void StatusLED::schedule() {
    scheduled = true; 
}

void StatusLED::update() {

    if (!scheduled) {
        return;
    }
    scheduled = false;

#ifndef BOOTLOADER

    readPowerState();

    PowerClass pclass = PSE_TYPE_INVALID;
    if (!powergood_state) {
    pclass = PSE_TYPE_POWER_BAD;
    } else {
    pclass = (PowerClass)(((tph_state)?0x4:0x0)|
                          ((tpl_state)?0x2:0x0)|
                          (( bt_state)?0x0:0x1));
    }
    if (power_class != pclass) {
        power_class = pclass;
        switch(power_class) {
        default:
            setUserLED(0x00, 0x00, 0x0f);
            break;
        case PSE_TYPE_3_4_CLASS_0_3:
        case PSE_TYPE_1_2_CLASS_0_3:
            DEBUG_PRINTF(("POE Power Class 0-3 (0-12.5W)\n"));
            setUserLED(0x0f, 0x07, 0x00);
            break;
        case PSE_TYPE_3_4_CLASS_4:
        case PSE_TYPE_2_CLASS_4:
            DEBUG_PRINTF(("POE Power Class 4 (0-25W)\n"));
            setUserLED(0x0f, 0x0f, 0x00);
            break;
        case PSE_TYPE_3_4_CLASS_5_6:
            DEBUG_PRINTF(("POE Power Class 5-6 (0-50W)\n"));
            setUserLED(0x07, 0x0f, 0x00);
            break;
        case PSE_TYPE_4_CLASS_7_8:
            DEBUG_PRINTF(("POE Power Class 7-8 (0-70W)\n"));
            setUserLED(0x00, 0x0f, 0x00);
            break;
        }
    }
#else  // #ifndef BOOTLOADER

    uint8_t v = Systick::instance().systemTime() & 0xFF;
    if (v > 0x7F) v = 0xff - v;
    
    switch (status) {
        case waiting:
        setUserLED(v, 0x00, 0x00);
        break;
        case erasing:
        setUserLED(v, 0x00, v);
        break;
        case flashing:
        setUserLED(0x00, 0x00, v);
        break;
        default:
        case done:
        setUserLED(0x00, v, 0x00);
        break;
        case reset:
        setUserLED(0x00, 0x00, 0x00);
        break;
    }
    
#endif  // #ifndef BOOTLOADER
}

__attribute__ ((hot, optimize("O2")))
void StatusLED::setUserLED(uint8_t r, uint8_t g, uint8_t b) {

    __disable_irq();

    uint32_t bits = (uint32_t(g)<<16) | (uint32_t(r)<<8) | uint32_t(b);
    for (int32_t d=23; d>=0; d--) {
#define SET GPIO_BOP(GPIOB) = GPIO_PIN_6;
#define RST GPIO_BC(GPIOB) = GPIO_PIN_6;
#define SEG 8
    if ((1UL<<d) & bits) {
            // one
            for (int32_t c=0; c<SEG*2; c++) {
                SET;
            }
            for (int32_t c=0; c<SEG*2; c++) {
                RST;
            }
        } else {
            // zero
            for (int32_t c=0; c<SEG*1; c++) {
                SET;
            }
            for (int32_t c=0; c<SEG*3; c++) {
                RST;
            }
        }
    }
    GPIO_BC(GPIOB) = GPIO_PIN_6;

    __enable_irq();
}

}  // namespace lightkraken {
