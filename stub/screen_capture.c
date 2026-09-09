/*******************************************************************************
*                                                                              *
* Screen capture stub for Windows                                              *
*                                                                              *
* screen_capture() is a Linux-only testing utility. This stub exists so that  *
* graphics_test can be linked on Windows without modification.                 *
*                                                                              *
*******************************************************************************/

void screen_capture(void) {}
void screen_capture_name(const char* fn) { (void)fn; }
void screen_capture_label(const char* s) { (void)s; }
