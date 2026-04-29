#include <obs-module.h>

#include <math.h>

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

#define OVERLAY_BASE_WIDTH 850.0f
#define OVERLAY_BASE_HEIGHT 400.0f

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

static uint32_t make_color_opaque(uint32_t color)
{
	return color | 0xFF000000;
}

static void draw_rect(gs_effect_t *effect, gs_eparam_t *color_param, float x, float y, float width,
			     float height, uint32_t color)
{
	if (!effect || !color_param || width <= 0.0f || height <= 0.0f) {
		return;
	}

	struct vec4 vec = color_to_vec4(make_color_opaque(color));
	gs_effect_set_vec4(color_param, &vec);

	gs_matrix_push();
	gs_matrix_translate3f(x, y, 0.0f);
	gs_matrix_scale3f(width, height, 1.0f);
	while (gs_effect_loop(effect, "Solid")) {
		gs_draw_sprite(NULL, 0, 1, 1);
	}
	gs_matrix_pop();
}

static void draw_rounded_rect(gs_effect_t *effect, gs_eparam_t *color_param, float x, float y, float width,
			      float height, float radius, uint32_t color)
{
	if (!effect || !color_param || width <= 0.0f || height <= 0.0f) {
		return;
	}

	float r = radius;
	if (r < 0.0f) {
		r = 0.0f;
	}
	if (r > width * 0.5f) {
		r = width * 0.5f;
	}
	if (r > height * 0.5f) {
		r = height * 0.5f;
	}

	if (r <= 0.5f) {
		draw_rect(effect, color_param, x, y, width, height, color);
		return;
	}

	const int strips = (int)fmaxf(24.0f, fminf(192.0f, ceilf(r * 2.2f)));
	for (int i = 0; i < strips; i++) {
		const float t0 = (float)i / (float)strips;
		const float t1 = (float)(i + 1) / (float)strips;
		const float x0 = t0 * width;
		const float x1 = t1 * width;
		const float xm = (x0 + x1) * 0.5f;

		float y_off = 0.0f;
		if (xm < r) {
			const float dx = r - xm;
			y_off = r - sqrtf((r * r) - (dx * dx));
		} else if (xm > (width - r)) {
			const float dx = xm - (width - r);
			y_off = r - sqrtf((r * r) - (dx * dx));
		}

		const float h = height - (y_off * 2.0f);
		if (h > 0.0f) {
			draw_rect(effect, color_param, x + x0, y + y_off, x1 - x0, h, color);
		}
	}
}

static float deg_to_rad(float degrees)
{
	return degrees * (3.1415926535f / 180.0f);
}

static float normalize_deg(float degrees)
{
	float value = fmodf(degrees, 360.0f);
	if (value < 0.0f) {
		value += 360.0f;
	}
	return value;
}

static float signed_angle_delta_deg(float from_deg, float to_deg)
{
	float delta = normalize_deg(to_deg) - normalize_deg(from_deg);
	if (delta > 180.0f) {
		delta -= 360.0f;
	} else if (delta < -180.0f) {
		delta += 360.0f;
	}
	return delta;
}

static bool angle_in_ccw_sweep(float start_deg, float end_deg, float angle_deg)
{
	const float start = normalize_deg(start_deg);
	const float end = normalize_deg(end_deg);
	const float angle = normalize_deg(angle_deg);
	float sweep = end - start;
	float offset = angle - start;

	if (sweep < 0.0f) {
		sweep += 360.0f;
	}
	if (offset < 0.0f) {
		offset += 360.0f;
	}

	return offset <= sweep;
}

static void draw_ring_arc(gs_effect_t *effect, gs_eparam_t *color_param, float cx, float cy, float inner_radius,
			  float outer_radius, float start_deg, float end_deg, float stamp_size,
			  float sampling_scale,
			  uint32_t color)
{
	if (!effect || !color_param || outer_radius <= inner_radius || stamp_size <= 0.0f) {
		return;
	}

	const float start_rad = deg_to_rad(start_deg);
	float sweep_deg = normalize_deg(end_deg) - normalize_deg(start_deg);
	if (sweep_deg <= 0.0f) {
		sweep_deg += 360.0f;
	}
	const float sweep_rad = deg_to_rad(sweep_deg);
	const float avg_radius = (inner_radius + outer_radius) * 0.5f;
	const float sample_scale = fmaxf(0.9f, fminf(2.0f, sampling_scale));
	const int angle_steps = (int)fmaxf(
		96.0f, fminf(640.0f, ceilf(((avg_radius * sweep_rad) / 1.2f) * sample_scale)));
	const int radial_steps = (int)fmaxf(
		10.0f, fminf(48.0f, ceilf(((outer_radius - inner_radius) / 0.8f) * sample_scale)));
	const float step_stamp = fmaxf(0.6f, fminf(1.25f, stamp_size * (0.72f / sample_scale)));

	for (int i = 0; i < angle_steps; i++) {
		const float t = ((float)i + 0.5f) / (float)angle_steps;
		const float angle = start_rad + sweep_rad * t;
		const float cs = cosf(angle);
		const float sn = sinf(angle);

		for (int j = 0; j < radial_steps; j++) {
			const float rt = ((float)j + 0.5f) / (float)radial_steps;
			const float r = inner_radius + (outer_radius - inner_radius) * rt;
			const float tangent_jitter = ((j & 1) ? 0.2f : -0.2f) * step_stamp;
			const float x = cx + (cs * r) - (sn * tangent_jitter) - (step_stamp * 0.5f);
			const float y = cy + (sn * r) + (cs * tangent_jitter) - (step_stamp * 0.5f);
			draw_rect(effect, color_param, x, y, step_stamp, step_stamp, color);
		}
	}
}

static void draw_rotating_sector(gs_effect_t *effect, gs_eparam_t *color_param, float cx, float cy,
				 float radius, float center_deg, float span_deg, float sampling_scale,
				 uint32_t color)
{
	if (!effect || !color_param || radius <= 0.0f || span_deg <= 0.0f) {
		return;
	}

	const float half_span = span_deg * 0.5f;
	const float start_deg = center_deg - half_span;
	const float end_deg = center_deg + half_span;
	const float sample_scale = fmaxf(0.8f, fminf(2.2f, sampling_scale));
	const float strip_width = fmaxf(0.65f, fminf(1.2f, 1.0f / sample_scale));
	const float y_step = fmaxf(0.6f, fminf(1.1f, 0.9f / sample_scale));
	const float x_min = cx - radius;
	const float x_max = cx + radius;
	const float r2 = radius * radius;

	for (float x = x_min; x < x_max; x += strip_width) {
		const float w = fminf(strip_width, x_max - x);
		const float xs = x + w * 0.5f;
		const float dx = xs - cx;
		const float dx2 = dx * dx;

		if (dx2 >= r2) {
			continue;
		}

		const float y_extent = sqrtf(r2 - dx2);
		const float y_min = cy - y_extent;
		const float y_max = cy + y_extent;
		bool in_run = false;
		float run_start = y_min;

		for (float y = y_min; y < y_max; y += y_step) {
			const float h = fminf(y_step, y_max - y);
			const float ys = y + h * 0.5f;
			float angle_deg = atan2f(ys - cy, dx) * (180.0f / 3.1415926535f);
			if (angle_deg < 0.0f) {
				angle_deg += 360.0f;
			}

			const bool inside = angle_in_ccw_sweep(start_deg, end_deg, angle_deg);
			if (inside) {
				if (!in_run) {
					in_run = true;
					run_start = y;
				}
			} else if (in_run) {
				draw_rect(effect, color_param, x, run_start, w, y - run_start, color);
				in_run = false;
			}
		}

		if (in_run) {
			draw_rect(effect, color_param, x, run_start, w, y_max - run_start, color);
		}
	}
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

static uint32_t get_fitted_dim(float base_dim, float scale)
{
	const float value = fmaxf(1.0f, base_dim * scale);
	return (uint32_t)lrintf(value);
}

static void load_visual_settings(struct input_overlay_source *ctx, obs_data_t *settings)
{
	const float requested_width = (float)obs_data_get_int(settings, "width");
	const float requested_height = (float)obs_data_get_int(settings, "height");
	const float scale_w = requested_width / OVERLAY_BASE_WIDTH;
	const float scale_h = requested_height / OVERLAY_BASE_HEIGHT;
	const float scale = fmaxf(0.35f, fmaxf(scale_w, scale_h));

	ctx->width = get_fitted_dim(OVERLAY_BASE_WIDTH, scale);
	ctx->height = get_fitted_dim(OVERLAY_BASE_HEIGHT, scale);
	ctx->background_color = make_color_opaque((uint32_t)obs_data_get_int(settings, "background_color"));
	ctx->active_color = make_color_opaque((uint32_t)obs_data_get_int(settings, "active_color"));
	ctx->inactive_color = make_color_opaque((uint32_t)obs_data_get_int(settings, "inactive_color"));
	ctx->alert_color = make_color_opaque((uint32_t)obs_data_get_int(settings, "alert_color"));
}

static const char *input_overlay_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("InputOverlaySource.Name");
}

static void *input_overlay_create(obs_data_t *settings, obs_source_t *source)
{
	struct input_overlay_source *ctx = bzalloc(sizeof(struct input_overlay_source));
	struct hid_backend_config config = get_backend_config(settings);
	ctx->source = source;
	load_visual_settings(ctx, settings);
	ctx->bridge = hid_backend_bridge_create(&config);
	if (!ctx->bridge) {
		blog(LOG_WARNING, "failed to allocate HID backend bridge");
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
	struct hid_backend_config config;
	if (!ctx) {
		return;
	}

	load_visual_settings(ctx, settings);
	config = get_backend_config(settings);
	if (!ctx->bridge) {
		ctx->bridge = hid_backend_bridge_create(&config);
	}
	if (ctx->bridge && !hid_backend_bridge_reconfigure(ctx->bridge, &config)) {
		blog(LOG_WARNING, "failed to start HID backend for OBS overlay source");
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
	const float render_scale = fminf(width / OVERLAY_BASE_WIDTH, height / OVERLAY_BASE_HEIGHT);
	const float pad = height * 0.05f;
	const float header_height = height * 0.12f;
	const float button_gap = height * 0.055f;
	const float button_height = height * 0.30f;
	const float button_width = button_height * (180.0f / 270.0f);
	const float button_corner_radius = 10.0f;
	const float row_gap = button_gap;
	const float button_top_y = pad + header_height + pad * 0.25f;
	const float button_bottom_y = button_top_y + button_height + row_gap;
	const float bottom_row_width = (button_width * 4.0f) + (button_gap * 3.0f);
	const float dial_diameter = (button_height * 2.0f) + row_gap;
	const float dial_radius = dial_diameter * 0.5f;
	const float arc_inner_radius = dial_radius + (height * 0.035f);
	const float arc_outer_radius = arc_inner_radius + (height * 0.025f);
	const float dial_border_thickness = fmaxf(1.0f, height * 0.01f);
	const float arc_overhang = fmaxf(0.0f, arc_outer_radius - dial_radius);
	const float dial_x = pad + arc_overhang + dial_border_thickness;
	const float dial_y = button_top_y;
	const float dial_right = dial_x + dial_diameter;
	const float dial_cx = dial_x + dial_radius;
	const float dial_cy = dial_y + dial_radius;
	const uint32_t dial_border_color = 0xFFB9C0CB;
	const float dial_sector_angle = -90.0f + clamp01(ctx->state.x_norm) * 360.0f;
	const float arc_stamp_size = fmaxf(0.75f, height * 0.006f);
	const float bottom_row_x = dial_right + (button_gap * 4.0f);
	const float top_row_x = bottom_row_x + (button_width + button_gap) * 0.5f;
	const float connection_dot_diameter = height * 0.06f;
	const float connection_dot_x = fminf(width - pad - connection_dot_diameter,
					     bottom_row_x + bottom_row_width + button_gap);
	const float connection_dot_y = pad;

	const uint32_t status_color = ctx->state.connected ? ctx->active_color : ctx->alert_color;
	const uint32_t ccw_arc_color = ctx->state.x_direction < 0 ? ctx->active_color : ctx->inactive_color;
	const uint32_t cw_arc_color = ctx->state.x_direction > 0 ? ctx->active_color : ctx->inactive_color;
	const uint32_t sector_color = dial_border_color;
	const bool buttons[7] = {
		ctx->state.button_01_pressed,
		ctx->state.button_02_pressed,
		ctx->state.button_03_pressed,
		ctx->state.button_04_pressed,
		ctx->state.button_05_pressed,
		ctx->state.button_06_pressed,
		ctx->state.button_07_pressed,
	};
	const size_t bottom_row_indices[4] = {0, 2, 4, 6};
	const size_t top_row_indices[3] = {1, 3, 5};

	draw_rect(solid, color_param, 0.0f, 0.0f, width, height, ctx->background_color);
	draw_rounded_rect(solid, color_param, connection_dot_x, connection_dot_y, connection_dot_diameter,
			  connection_dot_diameter, connection_dot_diameter * 0.5f, status_color);
	draw_rounded_rect(solid, color_param, dial_x, dial_y, dial_diameter, dial_diameter, dial_radius,
		  ctx->inactive_color);
	draw_rotating_sector(solid, color_param, dial_cx, dial_cy, dial_radius, dial_sector_angle, 10.0f,
			    render_scale,
			    sector_color);
	draw_ring_arc(solid, color_param, dial_cx, dial_cy, arc_inner_radius, arc_outer_radius, 183.0f, 357.0f,
		      arc_stamp_size, render_scale, ccw_arc_color);
	draw_ring_arc(solid, color_param, dial_cx, dial_cy, arc_inner_radius, arc_outer_radius, 3.0f, 177.0f,
		      arc_stamp_size, render_scale, cw_arc_color);

	for (size_t i = 0; i < 4; i++) {
		const size_t button_index = bottom_row_indices[i];
		const float x = bottom_row_x + (button_width + button_gap) * (float)i;
		const uint32_t color = buttons[button_index] ? ctx->active_color : ctx->inactive_color;
		draw_rounded_rect(solid, color_param, x, button_bottom_y, button_width, button_height,
			  button_corner_radius, color);
	}

	for (size_t i = 0; i < 3; i++) {
		const size_t button_index = top_row_indices[i];
		const float x = top_row_x + (button_width + button_gap) * (float)i;
		const uint32_t color = buttons[button_index] ? ctx->active_color : ctx->inactive_color;
		draw_rounded_rect(solid, color_param, x, button_top_y, button_width, button_height,
			  button_corner_radius, color);
	}

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
	obs_data_set_default_int(settings, "width", (long long)OVERLAY_BASE_WIDTH);
	obs_data_set_default_int(settings, "height", (long long)OVERLAY_BASE_HEIGHT);
	obs_data_set_default_int(settings, "background_color", 0xFF12141A);
	obs_data_set_default_int(settings, "active_color", 0xFF2ED18B);
	obs_data_set_default_int(settings, "inactive_color", 0xFF545D6B);
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
