#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct hid_backend_config {
	uint16_t vid;
	uint16_t pid;
	uint32_t button_usage_page;
	uint32_t button_01_usage;
	uint32_t button_02_usage;
	uint32_t button_03_usage;
	uint32_t button_04_usage;
	uint32_t button_05_usage;
	uint32_t button_06_usage;
	uint32_t button_07_usage;
	uint32_t button_link_collection;
	uint32_t axis_usage_page;
	uint32_t x_usage;
	uint32_t axis_link_collection;
	int32_t x_logical_min;
	int32_t x_logical_max;
	uint32_t x_idle_timeout_ms;
};

struct hid_backend_state {
	bool connected;
	bool button_01_pressed;
	bool button_02_pressed;
	bool button_03_pressed;
	bool button_04_pressed;
	bool button_05_pressed;
	bool button_06_pressed;
	bool button_07_pressed;
	float x_norm;
	int32_t x_direction;
	uint64_t tick_ms;
};

struct hid_backend_bridge;

struct hid_backend_bridge *hid_backend_bridge_create(const struct hid_backend_config *config);
void hid_backend_bridge_destroy(struct hid_backend_bridge *bridge);
bool hid_backend_bridge_reconfigure(struct hid_backend_bridge *bridge, const struct hid_backend_config *config);
bool hid_backend_bridge_try_get_latest(struct hid_backend_bridge *bridge, struct hid_backend_state *out_state);

#ifdef __cplusplus
}
#endif