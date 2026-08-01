#include "../miles_tag_ii.h"

/**
 * Byte editor for the custom message data byte.
 *
 * A variable item cannot offer 256 values (its value count is a uint8_t), so the
 * full range gets its own byte input screen.
 */

/** Fired on save only, so backing out of the editor leaves the byte unchanged. */
static void miles_tag_scene_data_callback(void* context) {
    MilesTagApp* app = context;

    app->config.custom_data = app->byte_input_value;
    scene_manager_previous_scene(app->scene_manager);
}

void miles_tag_scene_data_on_enter(void* context) {
    MilesTagApp* app = context;

    app->byte_input_value = app->config.custom_data;

    byte_input_set_header_text(app->byte_input, "Data byte");
    byte_input_set_result_callback(
        app->byte_input, miles_tag_scene_data_callback, NULL, app, &app->byte_input_value, 1);

    view_dispatcher_switch_to_view(app->view_dispatcher, MilesTagViewByteInput);
}

bool miles_tag_scene_data_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void miles_tag_scene_data_on_exit(void* context) {
    UNUSED(context);
}
