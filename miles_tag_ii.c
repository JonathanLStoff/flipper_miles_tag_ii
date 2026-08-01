#include "miles_tag_ii.h"

#define TAG "MilesTagII"

/** Tick often enough that the TX counter looks live during a burst. */
#define MILES_TAG_TICK_PERIOD_MS 200

static bool miles_tag_custom_event_callback(void* context, uint32_t event) {
    MilesTagApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool miles_tag_back_event_callback(void* context) {
    MilesTagApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void miles_tag_tick_event_callback(void* context) {
    MilesTagApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

/** Runs on the TX thread - just wake the GUI up. */
static void miles_tag_tx_done_callback(void* context) {
    MilesTagApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, MilesTagCustomEventTxDone);
}

static MilesTagApp* miles_tag_app_alloc(void) {
    MilesTagApp* app = malloc(sizeof(MilesTagApp));

    miles_tag_settings_load(&app->config, &app->sweep_options);
    app->config_item_count = 0;
    app->byte_input_value = app->config.custom_data;

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&miles_tag_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, miles_tag_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, miles_tag_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, miles_tag_tick_event_callback, MILES_TAG_TICK_PERIOD_MS);

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        MilesTagViewVarItemList,
        variable_item_list_get_view(app->var_item_list));

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MilesTagViewSubmenu, submenu_get_view(app->submenu));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, MilesTagViewWidget, widget_get_view(app->widget));

    app->byte_input = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MilesTagViewByteInput, byte_input_get_view(app->byte_input));

    app->tx_view = miles_tag_tx_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MilesTagViewTx, miles_tag_tx_view_get_view(app->tx_view));

    app->sweep_view = miles_tag_sweep_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MilesTagViewSweep, miles_tag_sweep_view_get_view(app->sweep_view));

    app->tx = miles_tag_tx_alloc();
    miles_tag_tx_set_callback(app->tx, miles_tag_tx_done_callback, app);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void miles_tag_app_free(MilesTagApp* app) {
    furi_check(app);

    /* Stop the radio before tearing down anything it might touch. */
    miles_tag_tx_free(app->tx);

    view_dispatcher_remove_view(app->view_dispatcher, MilesTagViewVarItemList);
    view_dispatcher_remove_view(app->view_dispatcher, MilesTagViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, MilesTagViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, MilesTagViewByteInput);
    view_dispatcher_remove_view(app->view_dispatcher, MilesTagViewTx);
    view_dispatcher_remove_view(app->view_dispatcher, MilesTagViewSweep);

    variable_item_list_free(app->var_item_list);
    submenu_free(app->submenu);
    widget_free(app->widget);
    byte_input_free(app->byte_input);
    miles_tag_tx_view_free(app->tx_view);
    miles_tag_sweep_view_free(app->sweep_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t miles_tag_ii_app(void* p) {
    UNUSED(p);

    MilesTagApp* app = miles_tag_app_alloc();

    scene_manager_next_scene(app->scene_manager, MilesTagSceneMenu);
    view_dispatcher_run(app->view_dispatcher);

    miles_tag_settings_save(&app->config, &app->sweep_options);
    miles_tag_app_free(app);

    return 0;
}
