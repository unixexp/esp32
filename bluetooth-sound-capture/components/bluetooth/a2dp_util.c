#include <stdio.h>
#include "a2dp_util.h"
#include "esp_gap_bt_api.h"
#include "i2s_util.h"
#include "esp_a2dp_legacy_api.h"
#include "esp_log.h"

static const char *A2DP_LOG_TAG = "A2DP";

static uint32_t s_pkt_cnt = 0;                     
static const char *s_a2dp_conn_state_str[] = {"Disconnected", "Connecting", "Connected", "Disconnecting"};
static const char *s_a2dp_audio_state_str[] = {"Suspended", "Started"};

static void a2dp_state_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *a2dp) {
	switch(event) {
		case ESP_A2D_SEP_REG_STATE_EVT: {
			ESP_LOGI(A2DP_LOG_TAG, "Registering audio port (SEP). SEID: %d, State: %d", 
			             a2dp->a2d_sep_reg_stat.seid, 
			             a2dp->a2d_sep_reg_stat.reg_state);
			if (a2dp->a2d_sep_reg_stat.reg_state == ESP_A2D_SEP_REG_SUCCESS) {
				ESP_LOGI(A2DP_LOG_TAG, "Logic audio channel initialized");
			}
			break;
		}
		case ESP_A2D_CONNECTION_STATE_EVT: {
	        uint8_t *bda = a2dp->conn_stat.remote_bda;
	        ESP_LOGI(A2DP_LOG_TAG, "A2DP connection state: %s, [%02x:%02x:%02x:%02x:%02x:%02x]",
	                 s_a2dp_conn_state_str[a2dp->conn_stat.state], bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
	        if (a2dp->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
	            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
				close_i2s_channel();
	        } else if (a2dp->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
	            esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
				start_i2s_channel();
	        } else if (a2dp->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTING) {
				open_i2s_channel();
	        }                                                       
	        break;                                                    
	    }
	    case ESP_A2D_AUDIO_STATE_EVT: {
	        ESP_LOGI(A2DP_LOG_TAG, "A2DP audio state: %s", s_a2dp_audio_state_str[a2dp->audio_stat.state]);
	        if (ESP_A2D_AUDIO_STATE_STARTED == a2dp->audio_stat.state) {
	            s_pkt_cnt = 0;
	        }
	        break;
	    }
		case ESP_A2D_AUDIO_CFG_EVT: {
			update_i2s_channel_config(&a2dp->audio_cfg.mcc);
			break;
		}
		default: {
			ESP_LOGE(A2DP_LOG_TAG, "%s unhandled event: %d", __func__, event);
			break;
		}
	}
}

static void a2dp_pcm_data_cb(const uint8_t *data, uint32_t len) {
	
	/*
	Network level / IRQ callback
	
	Here, we receive decoded audio data (ex. SBC -> PCM) and
	push here data to RingBuffer by i2s_data_output function
	
	We should'nt run heavy load operation here, just push data to the buffer
	*/
	
    i2s_data_output(data, len);
	
	if (++s_pkt_cnt % 100 == 0) {
        ESP_LOGI(A2DP_LOG_TAG, "Audio packet count: %"PRIu32, s_pkt_cnt);
    }
}

void init_a2dp(void) {
	esp_a2d_register_callback(&a2dp_state_cb);
	esp_a2d_sink_register_data_callback(a2dp_pcm_data_cb);
	esp_a2d_sink_init();
}
