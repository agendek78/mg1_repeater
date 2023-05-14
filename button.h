/*
 * button.h
 *
 *  Created on: May 7, 2023
 *      Author: andy
 */

#ifndef BUTTON_H_
#define BUTTON_H_

#define BUTTON_LONG_PRESS_EV       0

void button_init(void);
void button_process(void);

void button_on_event(int num_of_press);

#endif /* BUTTON_H_ */
