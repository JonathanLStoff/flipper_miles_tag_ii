#include "../miles_tag_ii.h"

/** Drill-down for the 7-bit player ID, with the stock MilesTag display names. */

static void miles_tag_scene_player_callback(void* context, uint32_t index) {
    MilesTagApp* app = context;

    app->config.player_id = (uint8_t)index;
    scene_manager_previous_scene(app->scene_manager);
}

void miles_tag_scene_player_on_enter(void* context) {
    MilesTagApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Player ID");

    for(uint32_t id = 0; id < MILES_TAG_PLAYER_COUNT; id++) {
        char label[24];
        miles_tag_player_label((uint8_t)id, label, sizeof(label));
        submenu_add_item(app->submenu, label, id, miles_tag_scene_player_callback, app);
    }

    submenu_set_selected_item(app->submenu, app->config.player_id);
    view_dispatcher_switch_to_view(app->view_dispatcher, MilesTagViewSubmenu);
}

bool miles_tag_scene_player_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void miles_tag_scene_player_on_exit(void* context) {
    MilesTagApp* app = context;
    submenu_reset(app->submenu);
}
