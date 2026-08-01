#include "../miles_tag_ii.h"

/** Runs the sweep, reports where it has got to, and pauses on demand. */

/** Defined by the sweep config scene; shared so both show the same estimate. */
uint32_t miles_tag_sweep_eta_seconds(const MilesTagApp* app, uint32_t remaining);

/** "Player+Team @ all freqs" - a one-line reminder of what is being tried. */
static void miles_tag_sweep_plan(const MilesTagApp* app, char* out, size_t out_size) {
    /* Longest possible content is "Player+Team+Dmg". */
    char fields[16] = {0};
    size_t offset = 0;

    const struct {
        bool active;
        const char* name;
    } parts[] = {
        {app->config.type == MilesTagSignalShot && app->sweep_options.player, "Player"},
        {app->config.type == MilesTagSignalShot && app->sweep_options.team, "Team"},
        {app->config.type == MilesTagSignalShot && app->sweep_options.damage, "Dmg"},
        {app->config.type == MilesTagSignalCommand && app->sweep_options.command, "Cmd"},
    };

    for(size_t i = 0; i < COUNT_OF(parts); i++) {
        if(!parts[i].active) continue;
        int written = snprintf(
            fields + offset,
            sizeof(fields) - offset,
            (offset == 0) ? "%s" : "+%s",
            parts[i].name);
        if(written <= 0) break;
        offset += (size_t)written;
    }

    if(offset == 0) {
        /* Nothing marked unknown: the sweep is just the one configured packet. */
        strncpy(fields, "Fixed", sizeof(fields) - 1);
    }

    if(app->sweep_options.frequency) {
        snprintf(out, out_size, "%s @ all freqs", fields);
    } else {
        snprintf(
            out,
            out_size,
            "%s @ %lu kHz",
            fields,
            miles_tag_frequencies[app->config.frequency_index] / 1000UL);
    }
}

static void miles_tag_scene_sweep_run_refresh(MilesTagApp* app) {
    MilesTagSweepProgress progress;
    miles_tag_tx_sweep_progress(app->tx, &progress);

    char current[48];
    miles_tag_config_summary(&progress.current, current, sizeof(current));

    uint32_t remaining = (progress.total > progress.index) ? progress.total - progress.index : 0;

    miles_tag_sweep_view_set_progress(
        app->sweep_view,
        progress.running,
        progress.index,
        progress.total,
        current,
        progress.logged,
        miles_tag_sweep_eta_seconds(app, remaining));
}

/** OK: start a fresh sweep, pause a running one, or resume a paused one. */
static void miles_tag_scene_sweep_run_toggle(void* context) {
    MilesTagApp* app = context;

    MilesTagSweepProgress progress;
    miles_tag_tx_sweep_progress(app->tx, &progress);

    if(progress.running) {
        miles_tag_tx_sweep_pause(app->tx);
        notification_message(app->notifications, &sequence_blink_stop);
        notification_message(app->notifications, &sequence_single_vibro);
    } else if(miles_tag_tx_sweep_is_finished(app->tx)) {
        /* Nothing left to do until it is reset. */
        return;
    } else {
        char plan[40];
        miles_tag_sweep_plan(app, plan, sizeof(plan));
        miles_tag_tx_sweep_run(app->tx, plan);
        notification_message(app->notifications, &sequence_blink_start_magenta);
    }

    miles_tag_scene_sweep_run_refresh(app);
}

/** Left: rewind to the first combination. */
static void miles_tag_scene_sweep_run_reset(void* context) {
    MilesTagApp* app = context;

    miles_tag_tx_sweep_prepare(app->tx, &app->config, &app->sweep_options);
    miles_tag_scene_sweep_run_refresh(app);
}

void miles_tag_scene_sweep_run_on_enter(void* context) {
    MilesTagApp* app = context;

    char plan[40];
    miles_tag_sweep_plan(app, plan, sizeof(plan));
    miles_tag_sweep_view_set_plan(app->sweep_view, plan);

    /* A paused sweep is only resumable if the settings behind it are unchanged;
     * editing the config and coming back starts a new one instead. */
    if(!miles_tag_tx_sweep_matches(app->tx, &app->config, &app->sweep_options)) {
        miles_tag_tx_sweep_prepare(app->tx, &app->config, &app->sweep_options);
    }

    miles_tag_sweep_view_set_callbacks(
        app->sweep_view,
        miles_tag_scene_sweep_run_toggle,
        miles_tag_scene_sweep_run_reset,
        app);
    miles_tag_scene_sweep_run_refresh(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, MilesTagViewSweep);
}

bool miles_tag_scene_sweep_run_on_event(void* context, SceneManagerEvent event) {
    MilesTagApp* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        miles_tag_scene_sweep_run_refresh(app);
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MilesTagCustomEventTxDone) {
            /* The sweep stopped: either it ran out of combinations or it paused. */
            notification_message(app->notifications, &sequence_blink_stop);
            if(miles_tag_tx_sweep_is_finished(app->tx)) {
                notification_message(app->notifications, &sequence_success);
            }
            miles_tag_scene_sweep_run_refresh(app);
            return true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        /* Leaving the screen must not leave the radio running. */
        miles_tag_tx_sweep_pause(app->tx);
        notification_message(app->notifications, &sequence_blink_stop);
    }

    return false;
}

void miles_tag_scene_sweep_run_on_exit(void* context) {
    MilesTagApp* app = context;

    miles_tag_tx_sweep_pause(app->tx);
    miles_tag_sweep_view_set_callbacks(app->sweep_view, NULL, NULL, NULL);
}
