/* Client side stand-in for the screen capture test hook: capture is a
   display side feature, and the display is the server's. */
void screen_capture(void) { }
void screen_capture_name(const char* fn) { (void)fn; }
void screen_capture_label(const char* s) { (void)s; }
