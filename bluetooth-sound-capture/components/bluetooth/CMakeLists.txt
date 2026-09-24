idf_component_register(
	SRCS "bt_core.c" "a2dp_util.c" "i2s_util.c"
	INCLUDE_DIRS "include"
	REQUIRES nvs_flash bt esp_driver_i2s freertos esp_ringbuf
)
