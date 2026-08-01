#include "../miles_tag_ii.h"

/** Landing screen: send one known signal, or sweep the ones you don't know. */

typedef enum {
    MilesTagMenuItemSend,
    MilesTagMenuItemBruteForce,
    MilesTagMenuItemAbout,
} MilesTagMenuItem;

static void miles_tag_scene_menu_callback(void* context, uint32_t index) {
    MilesTagApp* app = context;

    switch(index) {
    case MilesTagMenuItemSend:
        scene_manager_next_scene(app->scene_manager, MilesTagSceneConfig);
        break;
    case MilesTagMenuItemBruteForce:
        scene_manager_next_scene(app->scene_manager, MilesTagSceneSweepConfig);
        break;
    case MilesTagMenuItemAbout:
        scene_manager_next_scene(app->scene_manager, MilesTagSceneAbout);
        break;
    default:
        break;
    }
}

void miles_tag_scene_menu_on_enter(void* context) {
    MilesTagApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, MILES_TAG_APP_NAME);
    submenu_add_item(
        app->submenu, "Send Signal", MilesTagMenuItemSend, miles_tag_scene_menu_callback, app);
    submenu_add_item(
        app->submenu,
        "Brute Force",
        MilesTagMenuItemBruteForce,
        miles_tag_scene_menu_callback,
        app);
    submenu_add_item(
        app->submenu, "About / Protocol", MilesTagMenuItemAbout, miles_tag_scene_menu_callback, app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, MilesTagSceneMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, MilesTagViewSubmenu);
}

bool miles_tag_scene_menu_on_event(void* context, SceneManagerEvent event) {
    MilesTagApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        /* Back on the landing screen leaves the app. */
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }

    return false;
}

void miles_tag_scene_menu_on_exit(void* context) {
    MilesTagApp* app = context;

    scene_manager_set_scene_state(
        app->scene_manager, MilesTagSceneMenu, submenu_get_selected_item(app->submenu));
    submenu_reset(app->submenu);
}
