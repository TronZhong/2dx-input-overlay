#include "hid-backend-bridge.h"

#include <cstring>
#include <new>

#ifdef _WIN32

#include <memory>

#include "../../hid_input_backend.h"
#include "../../my_hid_adapter.h"

struct hid_backend_bridge {
	std::unique_ptr<HidInputBackend> backend;
	hid_backend_config config {};
};

static bool hid_backend_config_equal(const hid_backend_config &left, const hid_backend_config &right)
{
	return std::memcmp(&left, &right, sizeof(hid_backend_config)) == 0;
}

static MyHidConfig to_native_config(const hid_backend_config *config)
{
	MyHidConfig native {};
	native.vid = config->vid;
	native.pid = config->pid;
	native.buttonUsagePage = static_cast<USAGE>(config->button_usage_page);
	native.button_01Usage = static_cast<USAGE>(config->button_01_usage);
	native.button_02Usage = static_cast<USAGE>(config->button_02_usage);
	native.button_03Usage = static_cast<USAGE>(config->button_03_usage);
	native.button_04Usage = static_cast<USAGE>(config->button_04_usage);
	native.button_05Usage = static_cast<USAGE>(config->button_05_usage);
	native.button_06Usage = static_cast<USAGE>(config->button_06_usage);
	native.button_07Usage = static_cast<USAGE>(config->button_07_usage);
	native.buttonLinkCollection = static_cast<ULONG>(config->button_link_collection);
	native.axisUsagePage = static_cast<USAGE>(config->axis_usage_page);
	native.xUsage = static_cast<USAGE>(config->x_usage);
	native.axisLinkCollection = static_cast<ULONG>(config->axis_link_collection);
	native.xLogicalMin = static_cast<LONG>(config->x_logical_min);
	native.xLogicalMax = static_cast<LONG>(config->x_logical_max);
	native.xIdleTimeoutMs = config->x_idle_timeout_ms;
	return native;
}

static bool hid_backend_bridge_start(struct hid_backend_bridge *bridge, const struct hid_backend_config *config)
{
	auto backend = std::make_unique<HidInputBackend>(to_native_config(config));
	if (!backend->start()) {
		return false;
	}

	bridge->backend = std::move(backend);
	bridge->config = *config;
	return true;
}

extern "C" {

struct hid_backend_bridge *hid_backend_bridge_create(const struct hid_backend_config *config)
{
	auto *bridge = new (std::nothrow) hid_backend_bridge();
	if (!bridge) {
		return nullptr;
	}

	if (config) {
		hid_backend_bridge_reconfigure(bridge, config);
	}

	return bridge;
}

void hid_backend_bridge_destroy(struct hid_backend_bridge *bridge)
{
	delete bridge;
}

bool hid_backend_bridge_reconfigure(struct hid_backend_bridge *bridge, const struct hid_backend_config *config)
{
	if (!bridge || !config) {
		return false;
	}

	if (bridge->backend && hid_backend_config_equal(bridge->config, *config)) {
		return true;
	}

	bridge->backend.reset();
	return hid_backend_bridge_start(bridge, config);
}

bool hid_backend_bridge_try_get_latest(struct hid_backend_bridge *bridge, struct hid_backend_state *out_state)
{
	if (!bridge || !out_state) {
		return false;
	}

	std::memset(out_state, 0, sizeof(*out_state));
	if (!bridge->backend) {
		return false;
	}

	HidOverlayState state {};
	if (!bridge->backend->tryGetLatest(state)) {
		return false;
	}

	out_state->connected = state.connected;
	out_state->button_01_pressed = state.button01;
	out_state->button_02_pressed = state.button02;
	out_state->button_03_pressed = state.button03;
	out_state->button_04_pressed = state.button04;
	out_state->button_05_pressed = state.button05;
	out_state->button_06_pressed = state.button06;
	out_state->button_07_pressed = state.button07;
	out_state->x_norm = state.xNorm;
	out_state->x_direction = state.xDirection;
	out_state->tick_ms = state.tickMs;
	return true;
}

}

#else

struct hid_backend_bridge {
	hid_backend_config config {};
};

extern "C" {

struct hid_backend_bridge *hid_backend_bridge_create(const struct hid_backend_config *config)
{
	auto *bridge = new (std::nothrow) hid_backend_bridge();
	if (!bridge) {
		return nullptr;
	}

	if (config) {
		bridge->config = *config;
	}

	return bridge;
}

void hid_backend_bridge_destroy(struct hid_backend_bridge *bridge)
{
	delete bridge;
}

bool hid_backend_bridge_reconfigure(struct hid_backend_bridge *bridge, const struct hid_backend_config *config)
{
	if (!bridge || !config) {
		return false;
	}

	bridge->config = *config;
	return false;
}

bool hid_backend_bridge_try_get_latest(struct hid_backend_bridge *bridge, struct hid_backend_state *out_state)
{
	if (!bridge || !out_state) {
		return false;
	}

	std::memset(out_state, 0, sizeof(*out_state));
	return false;
}

}

#endif