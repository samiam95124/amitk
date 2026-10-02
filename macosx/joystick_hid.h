/* Joystick via IOKit HID for Mac OS X: presents IOKit joysticks to the
   terminal backend as the Linux driver would, a pipe of js_event records per
   stick. See joystick_hid.c. */
#ifndef JOYSTICK_HID_H
#define JOYSTICK_HID_H

int  pa_hid_joy_open(int n);     /* read end of joystick n's pipe, or -1 */
int  pa_hid_joy_axes(int n);     /* its axis count (JSIOCGAXES) */
int  pa_hid_joy_buttons(int n);  /* its button count (JSIOCGBUTTONS) */
void pa_hid_joy_close(void);     /* stop the manager, close the pipes */

#endif
