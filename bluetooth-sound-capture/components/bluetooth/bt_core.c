#include <stdint.h>
#include <stdio.h>
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_gap_bt_api.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_log.h"
#include "bt_core.h"

static const char *BT_CORE_TAG = "BT_CORE";
static bool SSP_ENABLED = false;

void init_nvs(void) {
	esp_err_t ret;
	
	/* initialize NVS flash memory to store controller parameters like name, passwords, etc... */
	ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
}

void init_bluetooth_controller(void) {
	/* free BLE mode reserved memory */
	ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));
	
	/* initialize Bluetooth Controller with default configuration */
	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
	    ESP_LOGE(BT_CORE_TAG, "%s initialize controller ed", __func__);
	    return;
	}
	/* enable Bluetooth Controller in Classic Bluetooth mode */
	if (esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) != ESP_OK) {
	    ESP_LOGE(BT_CORE_TAG, "%s enable controller failed", __func__);
	    return;
	}
}

void init_bluedroid_host(const char device_name[], bool ssp, const uint8_t *pin_ptr, uint8_t pin_len) {

	SSP_ENABLED = ssp;
	esp_err_t ret;
	esp_bluedroid_config_t bd_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
	bd_cfg.ssp_en = ssp;
	if ((ret = esp_bluedroid_init_with_cfg(&bd_cfg)) != ESP_OK) {
        ESP_LOGE(BT_CORE_TAG, "%s initialize bluedroid failed: %s", __func__,
			esp_err_to_name(ret));
        return;
    }

	/* enable Bluedroid Host */
    if (esp_bluedroid_enable() != ESP_OK) {
        ESP_LOGE(BT_CORE_TAG, "%s enable bluedroid failed", __func__);
        return;
    }
	
	esp_bt_gap_register_callback(bluedroid_gap_callback);
	
	if (SSP_ENABLED == true) {
		esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
		esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
		esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));	
		ESP_LOGI(BT_CORE_TAG, "Pairing security mode: SSP");
	} else {
		if (pin_ptr != NULL && pin_len > 0) {
			esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_FIXED, pin_len,(uint8_t*)pin_ptr);	
			ESP_LOGE(BT_CORE_TAG, "Pairing security mode: pin (%.*s)", pin_len, (char*)pin_ptr);
		}
	}
	
	esp_bt_gap_set_device_name(device_name);
	esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
}

static void bluedroid_dev_callback(esp_bt_dev_cb_event_t event, esp_bt_dev_cb_param_t *param) {
	switch (event) {
	    case ESP_BT_DEV_NAME_RES_EVT: {
	        if (param->name_res.status == ESP_BT_STATUS_SUCCESS) {
	            ESP_LOGI(BT_CORE_TAG, "Get local device name success: %s", param->name_res.name);
	        } else {
	            ESP_LOGE(BT_CORE_TAG, "Get local device name failed, status: %d", param->name_res.status);
	        }
	        break;
	    }
	    default: {
	        ESP_LOGI(BT_CORE_TAG, "event: %d", event);
	        break;
	    }
	}
}

static void bluedroid_gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    uint8_t *bda = NULL;

    switch (event) {
	    /* when authentication completed, this event comes */
	    case ESP_BT_GAP_AUTH_CMPL_EVT: {
	        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
	            ESP_LOGI(BT_CORE_TAG, "authentication success: %s", param->auth_cmpl.device_name);
	            ESP_LOG_BUFFER_HEX(BT_CORE_TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
	        } else {
	            ESP_LOGE(BT_CORE_TAG, "authentication failed, status: %d", param->auth_cmpl.stat);
	        }
	        ESP_LOGI(BT_CORE_TAG, "link key type of current link is: %d", param->auth_cmpl.lk_type);
	        break;
	    }
	    case ESP_BT_GAP_ENC_CHG_EVT: {
	        char *str_enc[3] = {"OFF", "E0", "AES"};
	        bda = (uint8_t *)param->enc_chg.bda;
	        ESP_LOGI(BT_CORE_TAG, "Encryption mode to [%02x:%02x:%02x:%02x:%02x:%02x] changed to %s",
	                 bda[0], bda[1], bda[2], bda[3], bda[4], bda[5], str_enc[param->enc_chg.enc_mode]);
	        break;
    	}

		/* when Security Simple Pairing user confirmation requested, this event comes */
	    case ESP_BT_GAP_CFM_REQ_EVT:
			if (SSP_ENABLED) {
		        ESP_LOGI(BT_CORE_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %06"PRIu32, param->cfm_req.num_val);
		        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
			} else {
				ESP_LOGW(BT_CORE_TAG, "Got SSP event, but SSP is disabled!");
			}
	        break;
	    /* when Security Simple Pairing passkey notified, this event comes */
	    case ESP_BT_GAP_KEY_NOTIF_EVT:
			if (SSP_ENABLED) {
	        	ESP_LOGI(BT_CORE_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey: %06"PRIu32, param->key_notif.passkey);
			}
	        break;
	    /* when Security Simple Pairing passkey requested, this event comes */
	    case ESP_BT_GAP_KEY_REQ_EVT:
			if (SSP_ENABLED) {
		        ESP_LOGI(BT_CORE_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
			}
	        break;
		
	    /* when GAP mode changed, this event comes */
	    case ESP_BT_GAP_MODE_CHG_EVT:
	        ESP_LOGI(BT_CORE_TAG, "ESP_BT_GAP_MODE_CHG_EVT mode: %d, interval: %.2f ms",
	                 param->mode_chg.mode, param->mode_chg.interval * 0.625);
	        break;
	    /* when ACL connection completed, this event comes */
	    case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT:
	        bda = (uint8_t *)param->acl_conn_cmpl_stat.bda;
	        ESP_LOGI(BT_CORE_TAG, "ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT Connected to [%02x:%02x:%02x:%02x:%02x:%02x], status: 0x%x",
	                 bda[0], bda[1], bda[2], bda[3], bda[4], bda[5], param->acl_conn_cmpl_stat.stat);
	        break;
	    /* when ACL disconnection completed, this event comes */
	    case ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT:
	        bda = (uint8_t *)param->acl_disconn_cmpl_stat.bda;
	        ESP_LOGI(BT_CORE_TAG, "ESP_BT_GAP_ACL_DISC_CMPL_STAT_EVT Disconnected from [%02x:%02x:%02x:%02x:%02x:%02x], reason: 0x%x",
	                 bda[0], bda[1], bda[2], bda[3], bda[4], bda[5], param->acl_disconn_cmpl_stat.reason);
	        break;
	    /* others */
	    default: {
	        ESP_LOGI(BT_CORE_TAG, "event: %d", event);
	        break;
	    }
    }
}
