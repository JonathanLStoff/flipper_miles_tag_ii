#include "miles_tag_ii.h"

#include <storage/storage.h>
#include <toolbox/saved_struct.h>

#define TAG "MilesTagSettings"

/** Everything worth remembering between runs, in one blob. */
typedef struct {
    MilesTagConfig config;
    MilesTagSweepOptions sweep_options;
} MilesTagSettings;

/** Reject anything a corrupt or older file could hand us. */
static void miles_tag_settings_sanitize(MilesTagSettings* settings) {
    MilesTagConfig* config = &settings->config;

    if(config->type >= MilesTagSignalCount) config->type = MilesTagSignalShot;
    if(config->frequency_index >= miles_tag_frequency_count) config->frequency_index = 2;
    if(config->duty_index >= miles_tag_duty_cycle_count) config->duty_index = 2;

    config->player_id &= 0x7F;
    config->team_id &= 0x03;
    config->damage_index &= 0x0F;
    config->pickup_id &= 0x0F;

    if(config->amount < 1 || config->amount > 100) config->amount = 10;
    if(config->command >= MILES_TAG_COMMAND_COUNT) config->command = 0;
    if(config->custom_message_id < 0x80) config->custom_message_id |= 0x80;
    if(config->repeat < 1 || config->repeat > 10) config->repeat = 1;
    if(config->gap_ms < 10) config->gap_ms = 50;

    MilesTagSweepOptions* sweep = &settings->sweep_options;
    if(sweep->repeat < 1 || sweep->repeat > 10) sweep->repeat = 1;
    if(sweep->delay_ms > 1000) sweep->delay_ms = 1000;
}

void miles_tag_settings_load(MilesTagConfig* config, MilesTagSweepOptions* sweep_options) {
    furi_check(config);
    furi_check(sweep_options);

    /* Defaults: unknown player and team is the common case for a sweep. */
    miles_tag_config_init(config);
    memset(sweep_options, 0, sizeof(MilesTagSweepOptions));
    sweep_options->player = true;
    sweep_options->team = true;
    sweep_options->log = true;
    sweep_options->repeat = 1;
    sweep_options->delay_ms = 20;

    MilesTagSettings loaded;
    if(saved_struct_load(
           MILES_TAG_SETTINGS_PATH,
           &loaded,
           sizeof(MilesTagSettings),
           MILES_TAG_SETTINGS_MAGIC,
           MILES_TAG_SETTINGS_VERSION)) {
        miles_tag_settings_sanitize(&loaded);
        *config = loaded.config;
        *sweep_options = loaded.sweep_options;
    } else {
        FURI_LOG_I(TAG, "No saved settings, using defaults");
    }
}

void miles_tag_settings_save(
    const MilesTagConfig* config,
    const MilesTagSweepOptions* sweep_options) {
    furi_check(config);
    furi_check(sweep_options);

    /* The app data directory does not exist until something creates it. */
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    furi_record_close(RECORD_STORAGE);

    MilesTagSettings settings = {
        .config = *config,
        .sweep_options = *sweep_options,
    };

    if(!saved_struct_save(
           MILES_TAG_SETTINGS_PATH,
           &settings,
           sizeof(MilesTagSettings),
           MILES_TAG_SETTINGS_MAGIC,
           MILES_TAG_SETTINGS_VERSION)) {
        FURI_LOG_W(TAG, "Failed to save settings");
    }
}
