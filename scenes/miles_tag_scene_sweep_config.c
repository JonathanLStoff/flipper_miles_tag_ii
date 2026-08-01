#include "../miles_tag_ii.h"

/**
 * Brute-force setup: mark the fields you do not know, hold the rest fixed.
 *
 * Each "Unknown" field turns into a dimension of the sweep; the row underneath
 * shows how many packets that adds up to and roughly how long it will take.
 */

static const uint16_t miles_tag_sweep_delays[] = {0, 10, 20, 50, 100, 200, 500, 1000};
#define MILES_TAG_SWEEP_DELAY_COUNT COUNT_OF(miles_tag_sweep_delays)

/**
 * Per-field mode. Each field is independent: sweep the player while pinning the
 * team to one colour, sweep the team for one known player, or sweep both and get
 * the full product of the two.
 */
static const char* const miles_tag_sweep_toggle[] = {"Fixed", "Sweep all"};

static void miles_tag_scene_sweep_config_rebuild(MilesTagApp* app);

static void miles_tag_sweep_type_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->config.type = (MilesTagSignalType)index;
    variable_item_set_current_value_text(item, miles_tag_signal_names[index]);

    /* Shot and command packets expose different fields to sweep. */
    view_dispatcher_send_custom_event(app->view_dispatcher, MilesTagCustomEventRebuildConfig);
}

static void miles_tag_sweep_frequency_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    /* Values 0..n-1 pick one carrier; the last one sweeps them all. */
    if(index < miles_tag_frequency_count) {
        app->sweep_options.frequency = false;
        app->config.frequency_index = index;

        char text[12];
        snprintf(text, sizeof(text), "%lu kHz", miles_tag_frequencies[index] / 1000UL);
        variable_item_set_current_value_text(item, text);
    } else {
        app->sweep_options.frequency = true;
        variable_item_set_current_value_text(item, "All");
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, MilesTagCustomEventRebuildConfig);
}

static void miles_tag_sweep_player_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->sweep_options.player = (index == 1);
    variable_item_set_current_value_text(item, miles_tag_sweep_toggle[index]);
    view_dispatcher_send_custom_event(app->view_dispatcher, MilesTagCustomEventRebuildConfig);
}

static void miles_tag_sweep_team_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->sweep_options.team = (index == 1);
    variable_item_set_current_value_text(item, miles_tag_sweep_toggle[index]);
    view_dispatcher_send_custom_event(app->view_dispatcher, MilesTagCustomEventRebuildConfig);
}

static void miles_tag_sweep_damage_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->sweep_options.damage = (index == 1);
    variable_item_set_current_value_text(item, miles_tag_sweep_toggle[index]);
    view_dispatcher_send_custom_event(app->view_dispatcher, MilesTagCustomEventRebuildConfig);
}

static void miles_tag_sweep_command_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->sweep_options.command = (index == 1);
    variable_item_set_current_value_text(item, miles_tag_sweep_toggle[index]);
    view_dispatcher_send_custom_event(app->view_dispatcher, MilesTagCustomEventRebuildConfig);
}

/* Fixed values used for whatever is not being swept. */

static void miles_tag_sweep_fixed_player_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    app->config.player_id = variable_item_get_current_value_index(item);

    char text[24];
    miles_tag_player_label(app->config.player_id, text, sizeof(text));
    variable_item_set_current_value_text(item, text);
}

static void miles_tag_sweep_fixed_team_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->config.team_id = index;
    variable_item_set_current_value_text(item, miles_tag_team_names[index]);
}

static void miles_tag_sweep_fixed_damage_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->config.damage_index = index;

    char text[12];
    snprintf(text, sizeof(text), "%u", miles_tag_damage_values[index]);
    variable_item_set_current_value_text(item, text);
}

static void miles_tag_sweep_fixed_command_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->config.command = index;
    variable_item_set_current_value_text(item, miles_tag_command_names[index]);
}

static void miles_tag_sweep_repeat_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    app->sweep_options.repeat = variable_item_get_current_value_index(item) + 1;

    char text[8];
    snprintf(text, sizeof(text), "%u", app->sweep_options.repeat);
    variable_item_set_current_value_text(item, text);
    view_dispatcher_send_custom_event(app->view_dispatcher, MilesTagCustomEventRebuildConfig);
}

static void miles_tag_sweep_delay_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->sweep_options.delay_ms = miles_tag_sweep_delays[index];

    char text[12];
    snprintf(text, sizeof(text), "%u ms", app->sweep_options.delay_ms);
    variable_item_set_current_value_text(item, text);
    view_dispatcher_send_custom_event(app->view_dispatcher, MilesTagCustomEventRebuildConfig);
}

static void miles_tag_sweep_log_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->sweep_options.log = (index == 1);
    variable_item_set_current_value_text(item, index ? "On" : "Off");
}

static uint8_t miles_tag_sweep_delay_index(uint16_t delay_ms) {
    for(uint8_t i = 0; i < MILES_TAG_SWEEP_DELAY_COUNT; i++) {
        if(miles_tag_sweep_delays[i] >= delay_ms) return i;
    }
    return MILES_TAG_SWEEP_DELAY_COUNT - 1;
}

/** Rough wall-clock estimate: air time per packet plus the configured delay. */
uint32_t miles_tag_sweep_eta_seconds(const MilesTagApp* app, uint32_t remaining) {
    MilesTagPacket packet;
    miles_tag_encode(&app->config, &packet);

    uint8_t repeat = app->sweep_options.repeat ? app->sweep_options.repeat : 1;
    uint32_t per_combo_ms =
        (miles_tag_packet_duration_us(&packet) * repeat) / 1000U + app->sweep_options.delay_ms;

    return (remaining * per_combo_ms + 999U) / 1000U;
}

static void miles_tag_scene_sweep_config_rebuild(MilesTagApp* app) {
    VariableItemList* list = app->var_item_list;
    VariableItem* item;
    char text[24];

    variable_item_list_reset(list);
    app->config_item_count = 0;

    /* Carrier: one specific frequency, or all three. */
    item = variable_item_list_add(
        list, "Frequency", miles_tag_frequency_count + 1, miles_tag_sweep_frequency_changed, app);
    if(app->sweep_options.frequency) {
        variable_item_set_current_value_index(item, miles_tag_frequency_count);
        variable_item_set_current_value_text(item, "All");
    } else {
        variable_item_set_current_value_index(item, app->config.frequency_index);
        snprintf(
            text,
            sizeof(text),
            "%lu kHz",
            miles_tag_frequencies[app->config.frequency_index] / 1000UL);
        variable_item_set_current_value_text(item, text);
    }
    miles_tag_config_map_add(app, MilesTagConfigItemFrequency);

    item =
        variable_item_list_add(list, "Signal", MilesTagSignalCount, miles_tag_sweep_type_changed, app);
    variable_item_set_current_value_index(item, app->config.type);
    variable_item_set_current_value_text(item, miles_tag_signal_names[app->config.type]);
    miles_tag_config_map_add(app, MilesTagConfigItemType);

    if(app->config.type == MilesTagSignalShot) {
        /* Player: sweep all 128 IDs, or pin one down. */
        item = variable_item_list_add(list, "Player", 2, miles_tag_sweep_player_changed, app);
        variable_item_set_current_value_index(item, app->sweep_options.player ? 1 : 0);
        variable_item_set_current_value_text(item, miles_tag_sweep_toggle[app->sweep_options.player ? 1 : 0]);
        miles_tag_config_map_add(app, MilesTagConfigItemSweepPlayer);

        if(!app->sweep_options.player) {
            item = variable_item_list_add(
                list, " Player ID", MILES_TAG_PLAYER_COUNT, miles_tag_sweep_fixed_player_changed, app);
            variable_item_set_current_value_index(item, app->config.player_id);
            miles_tag_player_label(app->config.player_id, text, sizeof(text));
            variable_item_set_current_value_text(item, text);
            miles_tag_config_map_add(app, MilesTagConfigItemPlayer);
        }

        item = variable_item_list_add(list, "Team", 2, miles_tag_sweep_team_changed, app);
        variable_item_set_current_value_index(item, app->sweep_options.team ? 1 : 0);
        variable_item_set_current_value_text(item, miles_tag_sweep_toggle[app->sweep_options.team ? 1 : 0]);
        miles_tag_config_map_add(app, MilesTagConfigItemSweepTeam);

        if(!app->sweep_options.team) {
            item = variable_item_list_add(
                list, " Team color", MILES_TAG_TEAM_COUNT, miles_tag_sweep_fixed_team_changed, app);
            variable_item_set_current_value_index(item, app->config.team_id);
            variable_item_set_current_value_text(item, miles_tag_team_names[app->config.team_id]);
            miles_tag_config_map_add(app, MilesTagConfigItemTeam);
        }

        item = variable_item_list_add(list, "Damage", 2, miles_tag_sweep_damage_changed, app);
        variable_item_set_current_value_index(item, app->sweep_options.damage ? 1 : 0);
        variable_item_set_current_value_text(item, miles_tag_sweep_toggle[app->sweep_options.damage ? 1 : 0]);
        miles_tag_config_map_add(app, MilesTagConfigItemSweepDamage);

        if(!app->sweep_options.damage) {
            item = variable_item_list_add(
                list, " Damage", MILES_TAG_DAMAGE_COUNT, miles_tag_sweep_fixed_damage_changed, app);
            variable_item_set_current_value_index(item, app->config.damage_index);
            snprintf(text, sizeof(text), "%u", miles_tag_damage_values[app->config.damage_index]);
            variable_item_set_current_value_text(item, text);
            miles_tag_config_map_add(app, MilesTagConfigItemDamage);
        }
    } else if(app->config.type == MilesTagSignalCommand) {
        item = variable_item_list_add(list, "Command", 2, miles_tag_sweep_command_changed, app);
        variable_item_set_current_value_index(item, app->sweep_options.command ? 1 : 0);
        variable_item_set_current_value_text(item, miles_tag_sweep_toggle[app->sweep_options.command ? 1 : 0]);
        miles_tag_config_map_add(app, MilesTagConfigItemSweepCommand);

        if(!app->sweep_options.command) {
            item = variable_item_list_add(
                list,
                " Command",
                MILES_TAG_COMMAND_COUNT,
                miles_tag_sweep_fixed_command_changed,
                app);
            variable_item_set_current_value_index(item, app->config.command);
            variable_item_set_current_value_text(item, miles_tag_command_names[app->config.command]);
            miles_tag_config_map_add(app, MilesTagConfigItemCommand);
        }
    }

    /* Timing. */
    item = variable_item_list_add(list, "Repeat each", 10, miles_tag_sweep_repeat_changed, app);
    variable_item_set_current_value_index(item, app->sweep_options.repeat - 1);
    snprintf(text, sizeof(text), "%u", app->sweep_options.repeat);
    variable_item_set_current_value_text(item, text);
    miles_tag_config_map_add(app, MilesTagConfigItemRepeat);

    item = variable_item_list_add(
        list, "Delay", MILES_TAG_SWEEP_DELAY_COUNT, miles_tag_sweep_delay_changed, app);
    variable_item_set_current_value_index(item, miles_tag_sweep_delay_index(app->sweep_options.delay_ms));
    snprintf(text, sizeof(text), "%u ms", app->sweep_options.delay_ms);
    variable_item_set_current_value_text(item, text);
    miles_tag_config_map_add(app, MilesTagConfigItemGap);

    /* Records every combination tried, in order, so a hit can be traced back. */
    item = variable_item_list_add(list, "Log to file", 2, miles_tag_sweep_log_changed, app);
    variable_item_set_current_value_index(item, app->sweep_options.log ? 1 : 0);
    variable_item_set_current_value_text(item, app->sweep_options.log ? "On" : "Off");
    miles_tag_config_map_add(app, MilesTagConfigItemSweepLog);

    /* Size of the job, so nobody starts a 20 minute sweep by accident. */
    uint32_t total = miles_tag_sweep_total(&app->config, &app->sweep_options);
    uint32_t eta = miles_tag_sweep_eta_seconds(app, total);

    item = variable_item_list_add(list, "Packets", 1, NULL, app);
    snprintf(text, sizeof(text), "%lu", total);
    variable_item_set_current_value_text(item, text);
    miles_tag_config_map_add(app, MilesTagConfigItemSweepTotal);

    item = variable_item_list_add(list, "Est. time", 1, NULL, app);
    if(eta >= 60) {
        snprintf(text, sizeof(text), "%lum %lus", eta / 60, eta % 60);
    } else {
        snprintf(text, sizeof(text), "%lus", eta);
    }
    variable_item_set_current_value_text(item, text);
    miles_tag_config_map_add(app, MilesTagConfigItemSweepEta);

    variable_item_list_add(list, "Start sweep", 0, NULL, NULL);
    miles_tag_config_map_add(app, MilesTagConfigItemSweepStart);
}

static void miles_tag_scene_sweep_config_enter_callback(void* context, uint32_t index) {
    MilesTagApp* app = context;
    if(index >= app->config_item_count) return;

    /* Deferred on purpose - see MilesTagCustomEventItemBase. */
    view_dispatcher_send_custom_event(
        app->view_dispatcher, MilesTagCustomEventItemBase + app->config_item_map[index]);
}

void miles_tag_scene_sweep_config_on_enter(void* context) {
    MilesTagApp* app = context;

    miles_tag_scene_sweep_config_rebuild(app);
    variable_item_list_set_enter_callback(
        app->var_item_list, miles_tag_scene_sweep_config_enter_callback, app);
    variable_item_list_set_selected_item(
        app->var_item_list,
        scene_manager_get_scene_state(app->scene_manager, MilesTagSceneSweepConfig));

    view_dispatcher_switch_to_view(app->view_dispatcher, MilesTagViewVarItemList);
}

bool miles_tag_scene_sweep_config_on_event(void* context, SceneManagerEvent event) {
    MilesTagApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MilesTagCustomEventRebuildConfig) {
            uint8_t selected = variable_item_list_get_selected_item_index(app->var_item_list);
            miles_tag_scene_sweep_config_rebuild(app);
            if(selected >= app->config_item_count) selected = app->config_item_count - 1;
            variable_item_list_set_selected_item(app->var_item_list, selected);
            return true;
        }

        if(event.event >= MilesTagCustomEventItemBase) {
            switch(event.event - MilesTagCustomEventItemBase) {
            case MilesTagConfigItemPlayer:
                scene_manager_next_scene(app->scene_manager, MilesTagScenePlayer);
                return true;
            case MilesTagConfigItemCommand:
                scene_manager_next_scene(app->scene_manager, MilesTagSceneCommand);
                return true;
            case MilesTagConfigItemSweepStart:
                scene_manager_next_scene(app->scene_manager, MilesTagSceneSweepRun);
                return true;
            default:
                return true;
            }
        }
    }

    return false;
}

void miles_tag_scene_sweep_config_on_exit(void* context) {
    MilesTagApp* app = context;

    scene_manager_set_scene_state(
        app->scene_manager,
        MilesTagSceneSweepConfig,
        variable_item_list_get_selected_item_index(app->var_item_list));
    variable_item_list_reset(app->var_item_list);
}
