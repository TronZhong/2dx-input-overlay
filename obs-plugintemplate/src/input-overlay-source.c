#include <obs-module.h>

#include "hid-backend-bridge.h"

struct input_overlay_source {
	obs_source_t *source;
	uint32_t width;
	uint32_t height;
	uint32_t background_color;
	uint32_t active_color;
	uint32_t inactive_color;
	uint32_t alert_color;
	struct hid_backend_bridge *bridge;
	struct hid_backend_state state;
};

static float clamp01(float value)
{
	if (value < 0.0f) {
		return 0.0f;
	}
	if (value > 1.0f) {
		return 1.0f;
	}
	return value;
}

static struct vec4 color_to_vec4(uint32_t color)
{
	struct vec4 value = {
		((color >> 16) & 0xFF) / 255.0f,
		((color >> 8) & 0xFF) / 255.0f,
		((color >> 0) & 0xFF) / 255.0f,
		((color >> 24) & 0xFF) / 255.0f,
	};
	return value;
}

static void draw_rect(gs_effect_t *effect, gs_eparam_t *color_param, float x, float y, float width,
			     float height, uint32_t color)
{
	if (!effect || !color_param || width <= 0.0f || height <= 0.0f) {
		return;
	}

	struct vec4 vec = color_to_vec4(color);
	gs_effect_set_vec4(color_param, &vec);

	gs_matrix_push();
	gs_matrix_translate3f(x, y, 0.0f);
	gs_matrix_scale3f(width, height, 1.0f);
	while (gs_effect_loop(effect, "Solid")) {
		gs_draw_sprite(NULL, 0, 1, 1);
	}
	gs_matrix_pop();
}

static struct hid_backend_config get_backend_config(obs_data_t *settings)
{
	struct hid_backend_config config = {
		.vid = (uint16_t)obs_data_get_int(settings, "device_vid"),
		.pid = (uint16_t)obs_data_get_int(settings, "device_pid"),
		.button_usage_page = (uint32_t)obs_data_get_int(settings, "button_usage_page"),
		.button_01_usage = (uint32_t)obs_data_get_int(settings, "button_01_usage"),
		.button_02_usage = (uint32_t)obs_data_get_int(settings, "button_02_usage"),
		.button_03_usage = (uint32_t)obs_data_get_int(settings, "button_03_usage"),
		.button_04_usage = (uint32_t)obs_data_get_int(settings, "button_04_usage"),
		.button_05_usage = (uint32_t)obs_data_get_int(settings, "button_05_usage"),
		.button_06_usage = (uint32_t)obs_data_get_int(settings, "button_06_usage"),
		.button_07_usage = (uint32_t)obs_data_get_int(settings, "button_07_usage"),
		.button_link_collection = (uint32_t)obs_data_get_int(settings, "button_link_collection"),
		.axis_usage_page = (uint32_t)obs_data_get_int(settings, "axis_usage_page"),
		.x_usage = (uint32_t)obs_data_get_int(settings, "x_usage"),
		.axis_link_collection = (uint32_t)obs_data_get_int(settings, "axis_link_collection"),
		.x_logical_min = (int32_t)obs_data_get_int(settings, "x_logical_min"),
		.x_logical_max = (int32_t)obs_data_get_int(settings, "x_logical_max"),
		.x_idle_timeout_ms = (uint32_t)obs_data_get_int(settings, "x_idle_timeout_ms"),
	};
	return config;
}

static void load_visual_settings(struct input_overlay_source *ctx, obs_data_t *settings)
{
	ctx->width = (uint32_t)obs_data_get_int(settings, "width");
	ctx->height = (uint32_t)obs_data_get_int(settings, "height");
	ctx->background_color = (uint32_t)obs_data_get_int(settings, "background_color");
	ctx->active_color = (uint32_t)obs_data_get_int(settings, "active_color");
	ctx->inactive_color = (uint32_t)obs_data_get_int(settings, "inactive_color");
	ctx->alert_color = (uint32_t)obs_data_get_int(settings, "alert_color");
}

static const char *input_overlay_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("InputOverlaySource.Name");
}

static void *input_overlay_create(obs_data_t *settings, obs_source_t *source)
{
	struct input_overlay_source *ctx = bzalloc(sizeof(struct input_overlay_source));
	ctx->source = source;
	load_visual_settings(ctx, settings);
	ctx->bridge = hid_backend_bridge_create(&get_backend_config(settings));
	if (!ctx->bridge) {
		obs_log(LOG_WARNING, "failed to allocate HID backend bridge");
	}
	return ctx;
}

static void input_overlay_destroy(void *data)
{
	struct input_overlay_source *ctx = data;
	if (!ctx) {
		return;
	}

	hid_backend_bridge_destroy(ctx->bridge);
	bfree(ctx);
}

static void input_overlay_update(void *data, obs_data_t *settings)
{
	struct input_overlay_source *ctx = data;
	if (!ctx) {
		return;
	}

	load_visual_settings(ctx, settings);
	if (!ctx->bridge) {
		ctx->bridge = hid_backend_bridge_create(&get_backend_config(settings));
	}
	if (ctx->bridge && !hid_backend_bridge_reconfigure(ctx->bridge, &get_backend_config(settings))) {
		obs_log(LOG_WARNING, "failed to start HID backend for OBS overlay source");
	}
}

static uint32_t input_overlay_get_width(void *data)
{
	struct input_overlay_source *ctx = data;
	return ctx ? ctx->width : 0;
}

static uint32_t input_overlay_get_height(void *data)
{
	struct input_overlay_source *ctx = data;
	return ctx ? ctx->height : 0;
}

static void input_overlay_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);

	struct input_overlay_source *ctx = data;
	if (!ctx || !ctx->bridge) {
		return;
	}

	hid_backend_bridge_try_get_latest(ctx->bridge, &ctx->state);
}

static void input_overlay_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct input_overlay_source *ctx = data;
	if (!ctx || ctx->width == 0 || ctx->height == 0) {
		return;
	}

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	if (!solid) {
		return;
	}

	gs_eparam_t *color_param = gs_effect_get_param_by_name(solid, "color");
	if (!color_param) {
		return;
	}

	const float width = (float)ctx->width;
	const float height = (float)ctx->height;
	const float pad = height * 0.08f;
	const float header_height = height * 0.16f;
	const float button_gap = width * 0.014f;
	const float button_width = (width - (pad * 2.0f) - (button_gap * 6.0f)) / 7.0f;
	const float button_height = height * 0.30f;
	const float button_y = pad + header_height + pad * 0.35f;
	const float track_y = height - pad - (height * 0.18f);
	const float track_height = height * 0.12f;
	const float track_width = width - pad * 2.0f;
	const float marker_width = width * 0.03f;
	const float center_x = pad + track_width * 0.5f;
	const float marker_x = pad + clamp01(ctx->state.x_norm) * (track_width - marker_width);

	const uint32_t status_color = ctx->state.connected ? ctx->active_color : ctx->alert_color;
	const uint32_t direction_color = ctx->state.x_direction < 0 ? ctx->alert_color : ctx->active_color;
	const bool buttons[7] = {
		ctx->state.button_01_pressed,
		ctx->state.button_02_pressed,
		ctx->state.button_03_pressed,
		ctx->state.button_04_pressed,
		ctx->state.button_05_pressed,
		ctx->state.button_06_pressed,
		ctx->state.button_07_pressed,
	};

	draw_rect(solid, color_param, 0.0f, 0.0f, width, height, ctx->background_color);
	draw_rect(solid, color_param, pad, pad, width * 0.24f, header_height, status_color);
	draw_rect(solid, color_param, width - pad - width * 0.34f, pad, width * 0.34f, header_height,
		  ctx->inactive_color);

	for (size_t i = 0; i < 7; i++) {
		const float x = pad + (button_width + button_gap) * (float)i;
		const uint32_t color = buttons[i] ? ctx->active_color : ctx->inactive_color;
		draw_rect(solid, color_param, x, button_y, button_width, button_height, color);
	}

	draw_rect(solid, color_param, pad, track_y, track_width, track_height, ctx->inactive_color);
	draw_rect(solid, color_param, center_x - 1.5f, track_y - 4.0f, 3.0f, track_height + 8.0f, ctx->alert_color);

	if (ctx->state.connected && ctx->state.x_direction != 0) {
		if (ctx->state.x_direction > 0) {
			draw_rect(solid, color_param, center_x, track_y + 2.0f, (track_width * 0.5f) - 2.0f,
				  track_height - 4.0f, direction_color);
		} else {
			draw_rect(solid, color_param, pad, track_y + 2.0f, (track_width * 0.5f) - 2.0f,
				  track_height - 4.0f, direction_color);
		}
	}

	draw_rect(solid, color_param, marker_x, track_y - 6.0f, marker_width, track_height + 12.0f,
		  ctx->state.connected ? ctx->active_color : ctx->alert_color);
}

static obs_properties_t *input_overlay_properties(void *data)
{
	UNUSED_PARAMETER(data);

	obs_properties_t *props = obs_properties_create();
	obs_properties_add_int(props, "width", obs_module_text("InputOverlaySource.Width"), 64, 3840, 1);
	obs_properties_add_int(props, "height", obs_module_text("InputOverlaySource.Height"), 64, 2160, 1);
	obs_properties_add_color_alpha(props, "background_color", obs_module_text("InputOverlaySource.BackgroundColor"));
	obs_properties_add_color_alpha(props, "active_color", obs_module_text("InputOverlaySource.ActiveColor"));
	obs_properties_add_color_alpha(props, "inactive_color", obs_module_text("InputOverlaySource.InactiveColor"));
	obs_properties_add_color_alpha(props, "alert_color", obs_module_text("InputOverlaySource.AlertColor"));
	obs_properties_add_int(props, "device_vid", obs_module_text("InputOverlaySource.DeviceVid"), 0, 65535, 1);
	obs_properties_add_int(props, "device_pid", obs_module_text("InputOverlaySource.DevicePid"), 0, 65535, 1);
	obs_properties_add_int(props, "button_usage_page", obs_module_text("InputOverlaySource.ButtonUsagePage"), 0, 65535, 1);
	obs_properties_add_int(props, "button_link_collection", obs_module_text("InputOverlaySource.ButtonLinkCollection"), 0, 32, 1);
	obs_properties_add_int(props, "button_01_usage", obs_module_text("InputOverlaySource.Button01Usage"), 0, 65535, 1);
	obs_properties_add_int(props, "button_02_usage", obs_module_text("InputOverlaySource.Button02Usage"), 0, 65535, 1);
	obs_properties_add_int(props, "button_03_usage", obs_module_text("InputOverlaySource.Button03Usage"), 0, 65535, 1);
	obs_properties_add_int(props, "button_04_usage", obs_module_text("InputOverlaySource.Button04Usage"), 0, 65535, 1);
	obs_properties_add_int(props, "button_05_usage", obs_module_text("InputOverlaySource.Button05Usage"), 0, 65535, 1);
	obs_properties_add_int(props, "button_06_usage", obs_module_text("InputOverlaySource.Button06Usage"), 0, 65535, 1);
	obs_properties_add_int(props, "button_07_usage", obs_module_text("InputOverlaySource.Button07Usage"), 0, 65535, 1);
	obs_properties_add_int(props, "axis_usage_page", obs_module_text("InputOverlaySource.AxisUsagePage"), 0, 65535, 1);
	obs_properties_add_int(props, "x_usage", obs_module_text("InputOverlaySource.XUsage"), 0, 65535, 1);
	obs_properties_add_int(props, "axis_link_collection", obs_module_text("InputOverlaySource.AxisLinkCollection"), 0, 32, 1);
	obs_properties_add_int(props, "x_logical_min", obs_module_text("InputOverlaySource.XLogicalMin"), -65535, 65535, 1);
	obs_properties_add_int(props, "x_logical_max", obs_module_text("InputOverlaySource.XLogicalMax"), -65535, 65535, 1);
	obs_properties_add_int(props, "x_idle_timeout_ms", obs_module_text("InputOverlaySource.XIdleTimeoutMs"), 0, 1000, 1);
	return props;
}

static void input_overlay_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "width", 640);
	obs_data_set_default_int(settings, "height", 180);
	obs_data_set_default_int(settings, "background_color", 0xD912141A);
	obs_data_set_default_int(settings, "active_color", 0xFF2ED18B);
	obs_data_set_default_int(settings, "inactive_color", 0x88545D6B);
	obs_data_set_default_int(settings, "alert_color", 0xFFE05252);
	obs_data_set_default_int(settings, "device_vid", 0x034C);
	obs_data_set_default_int(settings, "device_pid", 0x0368);
	obs_data_set_default_int(settings, "button_usage_page", 0x09);
	obs_data_set_default_int(settings, "button_link_collection", 0);
	obs_data_set_default_int(settings, "button_01_usage", 0x01);
	obs_data_set_default_int(settings, "button_02_usage", 0x02);
	obs_data_set_default_int(settings, "button_03_usage", 0x03);
	obs_data_set_default_int(settings, "button_04_usage", 0x04);
	obs_data_set_default_int(settings, "button_05_usage", 0x05);
	obs_data_set_default_int(settings, "button_06_usage", 0x06);
	obs_data_set_default_int(settings, "button_07_usage", 0x07);
	obs_data_set_default_int(settings, "axis_usage_page", 0x01);
	obs_data_set_default_int(settings, "x_usage", 0x30);
	obs_data_set_default_int(settings, "axis_link_collection", 0);
	obs_data_set_default_int(settings, "x_logical_min", 0);
	obs_data_set_default_int(settings, "x_logical_max", 255);
	obs_data_set_default_int(settings, "x_idle_timeout_ms", 33);
}

static struct obs_source_info input_overlay_source_info = {
	.id = "input_overlay_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = input_overlay_get_name,
	.create = input_overlay_create,
	.destroy = input_overlay_destroy,
	.update = input_overlay_update,
	.get_width = input_overlay_get_width,
	.get_height = input_overlay_get_height,
	.video_tick = input_overlay_tick,
	.video_render = input_overlay_render,
	.get_properties = input_overlay_properties,
	.get_defaults = input_overlay_defaults,
};

void register_input_overlay_source(void)
{
	obs_register_source(&input_overlay_source_info);
}
