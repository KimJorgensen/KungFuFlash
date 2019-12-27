/*
 * usb.h
 *
 *  Created on: 25.10.2011
 *      Author: skoe
 */

#ifndef USB_H_
#define USB_H_

#include <stdint.h>

#define USB_ID      *((uint8_t*) 0xde08)
#define USB_STATUS  *((uint8_t*) 0xde09)
#define USB_DATA    *((uint8_t*) 0xde0a)

#define USB_RX_READY 0x80
#define USB_TX_READY 0x40

char* ef3usb_check_cmd(void);

uint16_t __fastcall__ ef3usb_receive_data(void* buffer, uint16_t size);

void __fastcall__ ef3usb_send_data(const void* data, uint16_t len);

void __fastcall__ ef3usb_send_str(const char* p);

void ef3usb_discard_rx(void);

/* these functions can be used after ef3usb_send_str("load"): */
uint16_t __fastcall__ ef3usb_fread(void* buffer, uint16_t size);
void* ef3usb_fload(void);
void ef3usb_fclose(void);

#endif /* USB_H_ */
