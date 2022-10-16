#ifndef INC_EPPG_USB_H_
#define INC_EPPG_USB_H_

void   line_state_callback(bool connected);
void   parse_usb_serial();
void   initWebUSB();

#endif