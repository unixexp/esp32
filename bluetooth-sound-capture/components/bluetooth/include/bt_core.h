#ifndef _BT_CORE_H_
#define _BT_CORE_H_

#include <stdio.h>
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"

void init_nvs(void);
void init_bluetooth_controller(void);
void init_bluedroid_host(const char device_name[], bool ssp, const uint8_t *pin_ptr, uint8_t pin_len);
static void bluedroid_gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
static void bluedroid_dev_callback(esp_bt_dev_cb_event_t event, esp_bt_dev_cb_param_t *param);

#endif /* _BT_CORE_H_ */
