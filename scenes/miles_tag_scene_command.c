#include "../miles_tag_ii.h"

/** Drill-down for the data byte of message 0x83 (section 2.2.4 of the spec). */

static void miles_tag_scene_command_callback(void* context, uint32_t index) {
    MilesTagApp* app = context;

    app->config.command = (uint8_t)index;
    scene_manager_previous_scene(app->scene_manager);
}

void miles_tag_scene_command_on_enter(void* context) {
    MilesTagApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Command");

    for(uint32_t i = 0; i < MILES_TAG_COMMAND_COUNT; i++) {
        char label[32];
        snprintf(label, sizeof(label), "%02lX %s", i, miles_tag_command_names[i]);
        submenu_add_item(app->submenu, label, i, miles_tag_scene_command_callback, app);
    }

    submenu_set_selected_item(app->submenu, app->config.command);
    view_dispatcher_switch_to_view(app->view_dispatcher, MilesTagViewSubmenu);
}

bool miles_tag_scene_command_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void miles_tag_scene_command_on_exit(void* context) {
    MilesTagApp* app = context;
    submenu_reset(app->submenu);
}
