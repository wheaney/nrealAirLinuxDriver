//
// Created by thejackimonster on 29.03.23.
//
// Copyright (c) 2023 thejackimonster. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "device4.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hidapi/hidapi.h>

#define MAX_PACKET_SIZE 64

device4_type* device4_open(device4_event_callback callback) {
	device4_type* device = (device4_type*) malloc(sizeof(device4_type));
	
	if (!device) {
		perror("Not allocated!\n");
		return NULL;
	}
	
	memset(device, 0, sizeof(device4_type));
	device->vendor_id 	= 0x3318;
	device->product_id 	= 0x0424;
	device->callback 	= callback;

	if (0 != hid_init()) {
		perror("Not initialized!\n");
		return device;
	}

	struct hid_device_info* info = hid_enumerate(
		device->vendor_id, 
		device->product_id
	);

	struct hid_device_info* it = info;
	while (it) {
		if (it->interface_number == 3) {
			device->handle = hid_open_path(it->path);
			break;
		}

		it = it->next;
	}

	hid_free_enumeration(info);

	if (!device->handle) {
		perror("No handle!\n");
		return device;
	}
	
	uint8_t initial_brightness_payload [16] = {
			0xfd, 0x1e, 0xb9, 0xf0,
			0x68, 0x11, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x03
	};
	
	int size = MAX_PACKET_SIZE;
	if (sizeof(initial_brightness_payload) < size) {
		size = sizeof(initial_brightness_payload);
	}
	
	int transferred = hid_write(
			device->handle,
			initial_brightness_payload,
			size
	);
	
	if (transferred != sizeof(initial_brightness_payload)) {
		perror("ERROR\n");
		return device;
	}
	
	return device;
}

static void device4_callback(device4_type* device,
							 uint32_t timestamp,
							 device4_event_type event,
							 uint8_t brightness,
							 const char* msg) {
	if (!device->callback) {
		return;
	}
	
	device->callback(timestamp, event, brightness, msg);
}

void device4_clear(device4_type* device) {
	device4_read(device, 0);
}

int device4_read(device4_type* device, int timeout) {
	if (!device) {
		return -1;
	}
	
	if (MAX_PACKET_SIZE != sizeof(device4_packet_type)) {
		perror("Not proper size!\n");
		return -2;
	}
	
	device4_packet_type packet;
	memset(&packet, 0, sizeof(device4_packet_type));
	
	int transferred = hid_read_timeout(
			device->handle,
			(uint8_t*) &packet,
			MAX_PACKET_SIZE,
			timeout
	);
	
	/*if (transferred == 0) {
		return 1;
	}*/
	
	if (MAX_PACKET_SIZE != transferred) {
		perror("Not expected issue!\n");
		return -3;
	}
	
	const uint32_t timestamp = packet.timestamp;
	const size_t data_len = (size_t) &(packet.data) - (size_t) &(packet.length);
	
	switch (packet.action) {
		case DEVICE4_ACTION_PASSIVE_POLL_START: {
			break;
		}
		case DEVICE4_ACTION_BRIGHTNESS_COMMAND: {
			const uint8_t brightness = packet.data[1];
			
			device->brightness = brightness;
			
			device4_callback(
					device,
					timestamp,
					DEVICE4_EVENT_BRIGHTNESS_SET,
					device->brightness,
					NULL
			);
			break;
		}
		case DEVICE4_ACTION_MANUAL_POLL_CLICK: {
			const uint8_t button = packet.data[0];
			const uint8_t brightness = packet.data[8];
			
			switch (button) {
				case DEVICE4_BUTTON_DISPLAY_TOGGLE:
					device->active = !device->active;
					
					if (device->active) {
						device4_callback(
								device,
								timestamp,
								DEVICE4_EVENT_SCREEN_ON,
								device->brightness,
								NULL
						);
					} else {
						device4_callback(
								device,
								timestamp,
								DEVICE4_EVENT_SCREEN_OFF,
								device->brightness,
								NULL
						);
					}
					break;
				case DEVICE4_BUTTON_BRIGHTNESS_UP:
					device->brightness = brightness;
					
					device4_callback(
							device,
							timestamp,
							DEVICE4_EVENT_BRIGHTNESS_UP,
							device->brightness,
							NULL
					);
					break;
				case DEVICE4_BUTTON_BRIGHTNESS_DOWN:
					device->brightness = brightness;
					
					device4_callback(
							device,
							timestamp,
							DEVICE4_EVENT_BRIGHTNESS_DOWN,
							device->brightness,
							NULL
					);
					break;
				default:
					break;
			}
			
			break;
		}
		case DEVICE4_ACTION_ACTIVE_POLL: {
			const char* text = packet.text;
			const size_t text_len = strlen(text);
			
			device->active = true;
			
			if (data_len + text_len != packet.length) {
				perror("Not matching length!\n");
				return -5;
			}
			
			device4_callback(
					device,
					timestamp,
					DEVICE4_EVENT_MESSAGE,
					device->brightness,
					text
			);
			break;
		}
		case DEVICE4_ACTION_PASSIVE_POLL_END: {
			break;
		}
		default:
			device4_callback(
					device,
					timestamp,
					DEVICE4_EVENT_UNKNOWN,
					device->brightness,
					NULL
			);
			break;
	}
	
	return 0;
}

void device4_close(device4_type* device) {
	if (!device) {
		return;
	}
	
	if (device->handle) {
		hid_close(device->handle);
		device->handle = NULL;
	}
	
	free(device);
}
