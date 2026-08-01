#include "../miles_tag_ii.h"

/**
 * The one screen that drives everything: carrier frequency, signal type, and
 * then only those parameters that mean something for the chosen type.
 */

/** Repeat gap presets, in milliseconds. */
static const uint8_t miles_tag_gap_values[] = {10, 20, 30, 50, 75, 100, 150, 200, 250};
#define MILES_TAG_GAP_COUNT COUNT_OF(miles_tag_gap_values)

static void miles_tag_scene_config_rebuild(MilesTagApp* app);

/** Remember which control lives on which row - the layout changes with the type. */
void miles_tag_config_map_add(MilesTagApp* app, MilesTagConfigItem item) {
    furi_check(app->config_item_count < MILES_TAG_CONFIG_ITEM_MAX);
    app->config_item_map[app->config_item_count++] = item;
}

/* ------------------------------------------------------------- change hooks */

static void miles_tag_config_frequency_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->config.frequency_index = index;

    char text[12];
    snprintf(text, sizeof(text), "%lu kHz", miles_tag_frequencies[index] / 1000UL);
    variable_item_set_current_value_text(item, text);
}

static void miles_tag_config_type_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->config.type = (MilesTagSignalType)index;
    variable_item_set_current_value_text(item, miles_tag_signal_names[index]);

    /* The set of rows below this one depends on the type, but rebuilding the
     * list from inside its own callback is not safe - defer it to the event
     * loop instead. */
    view_dispatcher_send_custom_event(app->view_dispatcher, MilesTagCustomEventRebuildConfig);
}

static void miles_tag_config_player_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    app->config.player_id = variable_item_get_current_value_index(item);

    char text[24];
    miles_tag_player_label(app->config.player_id, text, sizeof(text));
    variable_item_set_current_value_text(item, text);
}

static void miles_tag_config_team_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->config.team_id = index;
    variable_item_set_current_value_text(item, miles_tag_team_names[index]);
}

static void miles_tag_config_damage_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->config.damage_index = index;

    char text[12];
    snprintf(text, sizeof(text), "%u", miles_tag_damage_values[index]);
    variable_item_set_current_value_text(item, text);
}

static void miles_tag_config_amount_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    app->config.amount = variable_item_get_current_value_index(item) + 1;

    char text[8];
    snprintf(text, sizeof(text), "%u", app->config.amount);
    variable_item_set_current_value_text(item, text);
}

static void miles_tag_config_command_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->config.command = index;
    variable_item_set_current_value_text(item, miles_tag_command_names[index]);
}

static void miles_tag_config_pickup_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    app->config.pickup_id = variable_item_get_current_value_index(item);

    char text[8];
    snprintf(text, sizeof(text), "%u", app->config.pickup_id);
    variable_item_set_current_value_text(item, text);
}

static void miles_tag_config_custom_id_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    app->config.custom_message_id = 0x80 + variable_item_get_current_value_index(item);

    char text[8];
    snprintf(text, sizeof(text), "0x%02X", app->config.custom_message_id);
    variable_item_set_current_value_text(item, text);
}

static void miles_tag_config_duty_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->config.duty_index = index;
    variable_item_set_current_value_text(item, miles_tag_duty_cycle_names[index]);
}

static void miles_tag_config_repeat_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    app->config.repeat = variable_item_get_current_value_index(item) + 1;

    char text[8];
    snprintf(text, sizeof(text), "%u", app->config.repeat);
    variable_item_set_current_value_text(item, text);
}

static void miles_tag_config_gap_changed(VariableItem* item) {
    MilesTagApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->config.gap_ms = miles_tag_gap_values[index];

    char text[12];
    snprintf(text, sizeof(text), "%u ms", app->config.gap_ms);
    variable_item_set_current_value_text(item, text);
}

/* --------------------------------------------------------------- list build */

/** Nearest preset index for the stored gap, so a saved value always shows up. */
static uint8_t miles_tag_gap_index(uint8_t gap_ms) {
    for(uint8_t i = 0; i < MILES_TAG_GAP_COUNT; i++) {
        if(miles_tag_gap_values[i] >= gap_ms) return i;
    }
    return MILES_TAG_GAP_COUNT - 1;
}

static void miles_tag_scene_config_rebuild(MilesTagApp* app) {
    VariableItemList* list = app->var_item_list;
    VariableItem* item;
    char text[24];

    variable_item_list_reset(list);
    app->config_item_count = 0;

    /* --- always visible: the carrier and the kind of signal --- */

    item = variable_item_list_add(
        list, "Frequency", miles_tag_frequency_count, miles_tag_config_frequency_changed, app);
    variable_item_set_current_value_index(item, app->config.frequency_index);
    snprintf(
        text, sizeof(text), "%lu kHz", miles_tag_frequencies[app->config.frequency_index] / 1000UL);
    variable_item_set_current_value_text(item, text);
    miles_tag_config_map_add(app, MilesTagConfigItemFrequency);

    item = variable_item_list_add(
        list, "Signal", MilesTagSignalCount, miles_tag_config_type_changed, app);
    variable_item_set_current_value_index(item, app->config.type);
    variable_item_set_current_value_text(item, miles_tag_signal_names[app->config.type]);
    miles_tag_config_map_add(app, MilesTagConfigItemType);

    /* --- the drill-down: only what this signal type actually carries --- */

    switch(app->config.type) {
    case MilesTagSignalShot:
        item = variable_item_list_add(
            list, "Player ID", MILES_TAG_PLAYER_COUNT, miles_tag_config_player_changed, app);
        variable_item_set_current_value_index(item, app->config.player_id);
        miles_tag_player_label(app->config.player_id, text, sizeof(text));
        variable_item_set_current_value_text(item, text);
        miles_tag_config_map_add(app, MilesTagConfigItemPlayer);

        item = variable_item_list_add(
            list, "Team", MILES_TAG_TEAM_COUNT, miles_tag_config_team_changed, app);
        variable_item_set_current_value_index(item, app->config.team_id);
        variable_item_set_current_value_text(item, miles_tag_team_names[app->config.team_id]);
        miles_tag_config_map_add(app, MilesTagConfigItemTeam);

        item = variable_item_list_add(
            list, "Damage", MILES_TAG_DAMAGE_COUNT, miles_tag_config_damage_changed, app);
        variable_item_set_current_value_index(item, app->config.damage_index);
        snprintf(text, sizeof(text), "%u", miles_tag_damage_values[app->config.damage_index]);
        variable_item_set_current_value_text(item, text);
        miles_tag_config_map_add(app, MilesTagConfigItemDamage);
        break;

    case MilesTagSignalAddHealth:
    case MilesTagSignalAddRounds:
        item = variable_item_list_add(
            list,
            (app->config.type == MilesTagSignalAddHealth) ? "Health" : "Rounds",
            100,
            miles_tag_config_amount_changed,
            app);
        variable_item_set_current_value_index(item, app->config.amount - 1);
        snprintf(text, sizeof(text), "%u", app->config.amount);
        variable_item_set_current_value_text(item, text);
        miles_tag_config_map_add(app, MilesTagConfigItemAmount);
        break;

    case MilesTagSignalCommand:
        item = variable_item_list_add(
            list, "Command", MILES_TAG_COMMAND_COUNT, miles_tag_config_command_changed, app);
        variable_item_set_current_value_index(item, app->config.command);
        variable_item_set_current_value_text(item, miles_tag_command_names[app->config.command]);
        miles_tag_config_map_add(app, MilesTagConfigItemCommand);
        break;

    case MilesTagSignalClipsPickup:
    case MilesTagSignalHealthPickup:
    case MilesTagSignalFlagPickup:
        item = variable_item_list_add(
            list,
            (app->config.type == MilesTagSignalFlagPickup) ? "Flag ID" : "Box ID",
            16,
            miles_tag_config_pickup_changed,
            app);
        variable_item_set_current_value_index(item, app->config.pickup_id);
        snprintf(text, sizeof(text), "%u", app->config.pickup_id);
        variable_item_set_current_value_text(item, text);
        miles_tag_config_map_add(app, MilesTagConfigItemPickup);
        break;

    case MilesTagSignalCustom:
        item = variable_item_list_add(
            list, "Message ID", 128, miles_tag_config_custom_id_changed, app);
        variable_item_set_current_value_index(item, app->config.custom_message_id - 0x80);
        snprintf(text, sizeof(text), "0x%02X", app->config.custom_message_id);
        variable_item_set_current_value_text(item, text);
        miles_tag_config_map_add(app, MilesTagConfigItemCustomId);

        /* All 256 values do not fit in a variable item (its value count is a
         * uint8_t), so this row opens a byte editor instead. */
        item = variable_item_list_add(list, "Data byte", 1, NULL, app);
        snprintf(text, sizeof(text), "0x%02X", app->config.custom_data);
        variable_item_set_current_value_text(item, text);
        miles_tag_config_map_add(app, MilesTagConfigItemCustomData);
        break;

    default:
        break;
    }

    /* --- transmitter settings that apply to every signal type --- */

    item = variable_item_list_add(
        list, "Duty cycle", miles_tag_duty_cycle_count, miles_tag_config_duty_changed, app);
    variable_item_set_current_value_index(item, app->config.duty_index);
    variable_item_set_current_value_text(item, miles_tag_duty_cycle_names[app->config.duty_index]);
    miles_tag_config_map_add(app, MilesTagConfigItemDuty);

    item = variable_item_list_add(list, "Repeat", 10, miles_tag_config_repeat_changed, app);
    variable_item_set_current_value_index(item, app->config.repeat - 1);
    snprintf(text, sizeof(text), "%u", app->config.repeat);
    variable_item_set_current_value_text(item, text);
    miles_tag_config_map_add(app, MilesTagConfigItemRepeat);

    item = variable_item_list_add(list, "Repeat gap", MILES_TAG_GAP_COUNT, miles_tag_config_gap_changed, app);
    variable_item_set_current_value_index(item, miles_tag_gap_index(app->config.gap_ms));
    snprintf(text, sizeof(text), "%u ms", app->config.gap_ms);
    variable_item_set_current_value_text(item, text);
    miles_tag_config_map_add(app, MilesTagConfigItemGap);

    /* --- actions --- */

    variable_item_list_add(list, "Transmit", 0, NULL, NULL);
    miles_tag_config_map_add(app, MilesTagConfigItemTransmit);

    variable_item_list_add(list, "About / protocol", 0, NULL, NULL);
    miles_tag_config_map_add(app, MilesTagConfigItemAbout);
}

/* -------------------------------------------------------------- scene hooks */

static void miles_tag_scene_config_enter_callback(void* context, uint32_t index) {
    MilesTagApp* app = context;
    if(index >= app->config_item_count) return;

    /* Deferred on purpose - see MilesTagCustomEventItemBase. */
    view_dispatcher_send_custom_event(
        app->view_dispatcher, MilesTagCustomEventItemBase + app->config_item_map[index]);
}

void miles_tag_scene_config_on_enter(void* context) {
    MilesTagApp* app = context;

    miles_tag_scene_config_rebuild(app);
    variable_item_list_set_enter_callback(
        app->var_item_list, miles_tag_scene_config_enter_callback, app);
    variable_item_list_set_selected_item(
        app->var_item_list, scene_manager_get_scene_state(app->scene_manager, MilesTagSceneConfig));

    view_dispatcher_switch_to_view(app->view_dispatcher, MilesTagViewVarItemList);
}

bool miles_tag_scene_config_on_event(void* context, SceneManagerEvent event) {
    MilesTagApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MilesTagCustomEventRebuildConfig) {
            /* Keep the cursor on the Signal row the user just changed. */
            miles_tag_scene_config_rebuild(app);
            variable_item_list_set_selected_item(app->var_item_list, 1);
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
            case MilesTagConfigItemCustomData:
                scene_manager_next_scene(app->scene_manager, MilesTagSceneData);
                return true;
            case MilesTagConfigItemTransmit:
                scene_manager_next_scene(app->scene_manager, MilesTagSceneTx);
                return true;
            case MilesTagConfigItemAbout:
                scene_manager_next_scene(app->scene_manager, MilesTagSceneAbout);
                return true;
            default:
                return true;
            }
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_set_scene_state(app->scene_manager, MilesTagSceneConfig, 0);
    }

    return false;
}

void miles_tag_scene_config_on_exit(void* context) {
    MilesTagApp* app = context;

    scene_manager_set_scene_state(
        app->scene_manager,
        MilesTagSceneConfig,
        variable_item_list_get_selected_item_index(app->var_item_list));
    variable_item_list_reset(app->var_item_list);
}
