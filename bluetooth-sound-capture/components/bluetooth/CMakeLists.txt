idf_component_register(
	SRCS "bt_core.c" "a2dp_util.c"
	INCLUDE_DIRS "include"
	REQUIRES nvs_flash bt
)
