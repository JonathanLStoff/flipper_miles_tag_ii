#include "miles_tag_log.h"

#include <furi.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>

#define TAG "MilesTagLog"

#define MILES_TAG_LOG_PATH APP_DATA_PATH("sweep_log.csv")

/** Flush once the buffer passes this; keeps SD writes off the per-packet path. */
#define MILES_TAG_LOG_FLUSH_BYTES 1024

struct MilesTagLog {
    Storage* storage;
    File* file;
    FuriString* buffer;
    uint32_t count;
    bool open;
};

const char* miles_tag_log_path(void) {
    return MILES_TAG_LOG_PATH;
}

MilesTagLog* miles_tag_log_alloc(void) {
    MilesTagLog* log = malloc(sizeof(MilesTagLog));

    log->storage = NULL;
    log->file = NULL;
    log->buffer = furi_string_alloc();
    log->count = 0;
    log->open = false;

    return log;
}

void miles_tag_log_free(MilesTagLog* log) {
    furi_check(log);

    if(log->open) {
        miles_tag_log_close(log, log->count, false);
    }
    furi_string_free(log->buffer);
    free(log);
}

/** Push the buffered text out to the card and empty the buffer. */
static void miles_tag_log_flush(MilesTagLog* log) {
    if(!log->open || furi_string_empty(log->buffer)) return;

    storage_file_write(
        log->file, furi_string_get_cstr(log->buffer), furi_string_size(log->buffer));
    furi_string_reset(log->buffer);
}

bool miles_tag_log_open(MilesTagLog* log, const char* plan, uint32_t total, bool resumed) {
    furi_check(log);
    furi_check(plan);

    if(log->open) return true;

    log->storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(log->storage, APP_DATA_PATH(""));

    log->file = storage_file_alloc(log->storage);
    if(!storage_file_open(log->file, MILES_TAG_LOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        FURI_LOG_W(TAG, "Could not open %s", MILES_TAG_LOG_PATH);
        storage_file_free(log->file);
        log->file = NULL;
        furi_record_close(RECORD_STORAGE);
        log->storage = NULL;
        return false;
    }

    log->open = true;
    log->count = 0;
    furi_string_reset(log->buffer);

    DateTime now;
    furi_hal_rtc_get_datetime(&now);

    /* A header per session, so several sweeps in one file stay readable. */
    furi_string_cat_printf(
        log->buffer,
        "\r\n# MilesTag II TX - sweep %s %04u-%02u-%02u %02u:%02u:%02u\r\n"
        "# %s, %lu combinations\r\n"
        "# index,freq_hz,type,player_id,player_name,team_id,team_name,damage_hp,command_hex,command_name\r\n",
        resumed ? "resumed" : "started",
        now.year,
        now.month,
        now.day,
        now.hour,
        now.minute,
        now.second,
        plan,
        total);

    miles_tag_log_flush(log);
    return true;
}

void miles_tag_log_append(MilesTagLog* log, uint32_t index, const MilesTagConfig* config) {
    furi_check(log);
    furi_check(config);

    if(!log->open) return;

    uint32_t frequency = miles_tag_frequencies
        [config->frequency_index < miles_tag_frequency_count ? config->frequency_index : 0];

    if(config->type == MilesTagSignalShot) {
        char player[24];
        miles_tag_player_label(config->player_id, player, sizeof(player));

        /* The label is "007 Blaze"; the name alone is what belongs in its column. */
        const char* player_name = strchr(player, ' ');
        player_name = player_name ? player_name + 1 : "";

        furi_string_cat_printf(
            log->buffer,
            "%lu,%lu,Shot,%u,%s,%u,%s,%u,,\r\n",
            index,
            frequency,
            config->player_id,
            player_name,
            config->team_id,
            miles_tag_team_names[config->team_id & 0x03],
            miles_tag_damage_values[config->damage_index & 0x0F]);
    } else if(config->type == MilesTagSignalCommand) {
        furi_string_cat_printf(
            log->buffer,
            "%lu,%lu,Command,,,,,,0x%02X,%s\r\n",
            index,
            frequency,
            config->command,
            (config->command < MILES_TAG_COMMAND_COUNT) ?
                miles_tag_command_names[config->command] :
                "Unknown");
    } else {
        char summary[48];
        miles_tag_config_summary(config, summary, sizeof(summary));
        furi_string_cat_printf(
            log->buffer,
            "%lu,%lu,%s,,,,,,,%s\r\n",
            index,
            frequency,
            miles_tag_signal_names[config->type],
            summary);
    }

    log->count++;

    if(furi_string_size(log->buffer) >= MILES_TAG_LOG_FLUSH_BYTES) {
        miles_tag_log_flush(log);
    }
}

void miles_tag_log_close(MilesTagLog* log, uint32_t index, bool completed) {
    furi_check(log);

    if(!log->open) return;

    furi_string_cat_printf(
        log->buffer,
        "# %s after %lu combinations\r\n",
        completed ? "completed" : "paused",
        index);
    miles_tag_log_flush(log);

    storage_file_close(log->file);
    storage_file_free(log->file);
    log->file = NULL;

    furi_record_close(RECORD_STORAGE);
    log->storage = NULL;
    log->open = false;
}

bool miles_tag_log_is_open(const MilesTagLog* log) {
    furi_check(log);
    return log->open;
}

uint32_t miles_tag_log_count(const MilesTagLog* log) {
    furi_check(log);
    return log->count;
}
