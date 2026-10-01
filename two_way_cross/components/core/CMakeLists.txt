idf_component_register(
	SRCS "bt.c" "a2dp.c" "i2s.c" "fir.c"
	INCLUDE_DIRS "include"
	REQUIRES nvs_flash bt esp_driver_i2s freertos esp_ringbuf esp_dsp
)
