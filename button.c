/*
 * button.c
 *
 *  Created on: May 7, 2023
 *      Author: andy
 */

#include "button.h"
#include "dbg_log.h"

#include "app/framework/include/af.h"

#define BUTTON_LONG_PRESS_TIMEOUT   3000
#define BUTTON_MULTI_PRESS_TIMEOUT  1000

typedef enum
{
    BtnState_IDLE,
    BtnState_PRESSED,
    BtnState_RELEASED
} BtnState;

static BtnState button_state = BtnState_IDLE;
static int32_t button_ts;
static uint8_t btn_cnt = 0;

void button_init(void)
{

}

void button_process(void)
{
    if (button_state == BtnState_PRESSED &&
        (int32_t)halCommonGetInt32uMillisecondTick() - button_ts > BUTTON_LONG_PRESS_TIMEOUT)
    {
        DBG_LOG("Long press");
        button_on_event(BUTTON_LONG_PRESS_EV);
    }
    else if (button_state == BtnState_RELEASED)
    {
        button_state = BtnState_IDLE;
        btn_cnt++;
    }

    if (btn_cnt != 0 &&
        (int32_t)halCommonGetInt32uMillisecondTick() - button_ts > BUTTON_MULTI_PRESS_TIMEOUT)
    {
        button_on_event(btn_cnt);
        btn_cnt = 0;
    }
}

void emberAfHalButtonIsrCallback(int8u button,
                                 int8u state)
{
    if (state == BUTTON_PRESSED)
    {
        button_ts = halCommonGetInt32uMillisecondTick();
        button_state = BtnState_PRESSED;
    }
    else if (state == BUTTON_RELEASED && button_state == BtnState_PRESSED)
    {
        button_state = BtnState_RELEASED;
    }
}
