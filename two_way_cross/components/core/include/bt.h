#ifndef _BT_H_
#define _BT_H_

#include <stdio.h>
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"

void init_nvs(void);
void init_bluetooth_controller(void);
void init_bluedroid_host(const char device_name[], bool ssp, const uint8_t *pin_ptr, uint8_t pin_len);

#endif /* _BT_H_ */
